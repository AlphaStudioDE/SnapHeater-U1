"""Record local build provenance and artifact integrity, without credentials.

Run after building. This generates metadata only; it does not publish or flash.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args]).decode("utf-8").strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--apk", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    description = json.loads((build / "project_description.json").read_text())
    sdk = Path(description["idf_path"])
    cfg = Path(description["config_file"])
    sources = hashlib.sha256()
    for name in sorted(set(git(root, "ls-files", "--cached", "--others", "--exclude-standard", "-z").split("\0"))):
        path = root / name
        if name and path.is_file():
            sources.update(name.encode())
            sources.update(b"\0")
            sources.update(bytes.fromhex(sha(path)))
    artifacts = [build / "SnapHeater_U1.bin", build / "bootloader/bootloader.bin",
                 build / "partition_table/partition-table.bin"]
    if args.apk:
        artifacts.append(args.apk.resolve())
    result = {
        "firmware_version": description["project_version"],
        "git_revision": git(root, "rev-parse", "HEAD"),
        "source_tree_sha256": sources.hexdigest(),
        "source_dirty": bool(git(root, "status", "--porcelain")),
        "idf_version": git(sdk, "describe", "--tags", "--always", "--dirty"),
        "idf_revision": git(sdk, "rev-parse", "HEAD"),
        "idf_dirty": bool(git(sdk, "status", "--porcelain")),
        "sdkconfig_sha256": sha(cfg),
        "dependencies_lock_sha256": sha(root / "dependencies.lock"),
        "artifacts": {p.name: {"bytes": p.stat().st_size, "sha256": sha(p)} for p in artifacts},
        "qualification": "software-build-only; no hardware qualification",
    }
    target = build / "build-manifest.json"
    target.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(target)


if __name__ == "__main__":
    main()
