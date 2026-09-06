# Build provenance

Build with ESP-IDF **5.3.5** and the committed `dependencies.lock`. The managed
WebSocket client is pinned to **1.7.0**. Review intentional dependency changes
and regenerate the lock together; do not silently upgrade it for a release.
The firmware app descriptor and HTTP/BLE firmware version use the same version
from `main/app_config.h`. Android has its own version name and increasing code.

After building, generate an integrity/provenance record, for example:

```text
python tools/build_manifest.py --build build-panda-tester --apk apps/android/SnapHeaterU1/app/build/outputs/apk/debug/app-debug.apk
```

The generated `build-manifest.json` stays in the build directory. It records
image sizes and SHA-256, source revision/tree fingerprint, dirty-tree indicators,
SDK revision, and hashes of configuration and the dependency lock. It does not
export Wi-Fi passwords, tokens, local paths or configuration contents. It is not
a cryptographic signature or an endorsement of any binary supplied by a third party.

Release preparation must use a reviewed source snapshot and SDK, preserve the
selected hardware configuration, and retain the manifest alongside the binaries.
Do not publish local `sdkconfig` files, work notes, backups or credentials.
Source changes invalidate the previous build's provenance: rebuild and regenerate.
The local desktop SDK may have Windows symlink/type changes; the manifest reports
it as dirty rather than pretending it is a pristine upstream checkout.

This pins the build inputs and makes artifacts identifiable. A second clean,
independent build has not yet established bit-for-bit reproducibility. Offline
build/test success does not qualify electrical waveforms, cooling or stock
bootloader recovery on a physical Panda.
