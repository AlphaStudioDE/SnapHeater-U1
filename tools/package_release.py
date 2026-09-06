"""Package reviewed app-only firmware and Android testing APK; no device access.

Only the explicit allowlist below is exported. No sdkconfig, full-flash dump,
private work directory, signing key or build log is copied.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def public_links(text, folder, tag):
    def replace(match):
        label, link = match.groups()
        if ':' in link or link.startswith('#'):
            return match.group(0)
        # Resolve repository-relative documentation without leaking local paths.
        target = (ROOT / folder / link.split('#')[0]).resolve().relative_to(ROOT)
        anchor = '#' + link.split('#', 1)[1] if '#' in link else ''
        return f'[{label}](https://github.com/AlphaStudioDE/SnapHeater-U1/blob/{tag}/{target.as_posix()}{anchor})'
    return re.sub(r'\[([^\]]+)\]\(([^)]+)\)', replace, text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--apk', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--aapt', type=Path, required=True)
    args = parser.parse_args()
    version = re.search(r'#define SHU1_FW_VERSION\s+"([^"]+)"', (ROOT/'main/app_config.h').read_text()).group(1)
    assert version == '0.9.9', 'Review package naming/policy for each release'
    tag = 'v' + version
    image = args.build/'SnapHeater_U1.bin'
    raw = image.read_bytes()
    assert raw[0] == 0xE9 and len(raw) <= 0x1e0000
    assert raw[32:36] == bytes.fromhex('3254cdab'), 'ESP app descriptor missing'
    assert raw[48:80].split(b'\0')[0].decode() == version, 'Stale embedded version'
    assert raw[80:112].split(b'\0')[0] == b'SnapHeater_U1'
    provenance = json.loads((args.build/'build-manifest.json').read_text())
    assert provenance['firmware_version'] == version
    assert not provenance['source_dirty'], 'Commit reviewed sources before packaging'
    assert provenance['artifacts']['SnapHeater_U1.bin']['sha256'] == digest(image)
    assert provenance['artifacts']['app-debug.apk']['sha256'] == digest(args.apk)
    badging = subprocess.check_output([str(args.aapt), 'dump', 'badging', str(args.apk)], text=True)
    assert "versionName='0.9.9'" in badging and "versionCode='4'" in badging, 'Never package a local preview APK'
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)  # Never overwrite an existing release.
    bin_name = f'SnapHeater-U1-{version}-ota.bin'
    apk_name = f'SnapHeater-U1-Android-{version}-debug.apk'
    shutil.copyfile(image, out/bin_name)
    shutil.copyfile(args.apk, out/apk_name)
    documents = {
        'INSTALL.md': 'docs/INSTALL_0.9.9.md',
        'TESTING-RISK-NOTICE.md': 'docs/HARDWARE_LIABILITY_DISCLAIMER.md',
        'SAFETY-STATUS.md': 'docs/SAFETY_STATUS.md',
        'RELEASE-NOTES.md': 'docs/RELEASE_0.9.9.md',
        'LICENSE.txt': 'LICENSE',
        'THIRD-PARTY-NOTICES.md': 'THIRD_PARTY_NOTICES.md',
        'DragonBreath-MIT.txt': 'docs/licenses/DragonBreath-MIT.txt',
        'dragon-core-MIT.txt': 'docs/licenses/dragon-core-MIT.txt',
    }
    for name, source in documents.items():
        text = (ROOT/source).read_text(encoding='utf-8')
        (out/name).write_text(public_links(text, Path(source).parent, provenance['git_revision']), encoding='utf-8')
    provenance.update({
        'release_tag': tag, 'status': 'experimental-prerelease',
        'android_version': version, 'android_version_code': 4,
        'visual_preview_available': False,
        'android_build_type': 'debug/testing; not production-signed',
        'installation': 'app-only OTA; never flash application at 0x0',
        'artifact_files': {p.name: {'sha256': digest(p), 'bytes': p.stat().st_size}
                           for p in out.iterdir() if p.is_file()},
    })
    (out/'manifest.json').write_text(json.dumps(provenance, indent=2)+'\n', encoding='utf-8')
    # Internal checksums cover the package payload, not the ZIP itself.
    files = sorted(out.iterdir())
    sums = ''.join(f'{digest(p)}  {p.name}\n' for p in files)
    archive = out/f'SnapHeater-U1-{version}-tester.zip'
    with ZipFile(archive, 'w', ZIP_DEFLATED) as zipfile:
        for path in files:
            zipfile.write(path, path.name)
        zipfile.writestr('SHA256SUMS.txt', sums)
    (out/'SHA256SUMS.txt').write_text(sums+f'{digest(archive)}  {archive.name}\n', encoding='utf-8')
    with ZipFile(archive) as zipfile:
        assert zipfile.testzip() is None
    print(json.dumps({'directory': str(out), 'files': [p.name for p in out.iterdir()]}, indent=2))


if __name__ == '__main__':
    main()
