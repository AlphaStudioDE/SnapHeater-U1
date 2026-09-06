# Install and update — SnapHeater U1 0.9.9

**Experimental prerelease. Supervise every test. Not hardware-qualified.**
Read [risks and responsibility](HARDWARE_LIABILITY_DISCLAIMER.md) first.
Target: original Panda Breath ESP32-C3 V1.0/V1.0.1 electronics only.
Do not install this image on AirGuard 300 or a different controller.

## Files in the release

- `SnapHeater-U1-0.9.9-ota.bin`: **application only**, for an OTA updater.
- `SnapHeater-U1-Android-0.9.9-debug.apk`: Android 8+ testing companion.
- `SnapHeater-U1-0.9.9-tester.zip`: both files, instructions, notices and checksums.
- `SHA256SUMS.txt`: SHA-256 integrity values for release files.
- `manifest.json`: build versions, source reference and artifact hashes.

The BIN is not a full-flash backup. **Never write it at address 0x0.**
No bootloader, partition-table replacement or private device dump is shipped in
the tester package. The release does not silently erase NVS or calibration.

## Before any firmware change

1. Stop heating and all scheduled/paused work. Wait for cooling to finish.
2. Identify the exact device/board revision. Do not guess from a similar product name.
3. Keep a private backup of your own stock flash if you have safe access and the
   necessary skills. See [backup/recovery](FLASH_BACKUP_RESTORE.md).
   Full dumps can contain Wi-Fi passwords and pairing data: never post them.
4. Keep power/network stable throughout the update. Do not hold panel buttons at boot.
5. Do not open or probe mains circuitry unless qualified. If a backup/recovery
   procedure requires unsafe access, stop and seek qualified assistance.

Verify downloaded files against the release checksums, for example on Windows:

```powershell
Get-FileHash .\SnapHeater-U1-0.9.9-ota.bin -Algorithm SHA256
Get-FileHash .\SnapHeater-U1-Android-0.9.9-debug.apk -Algorithm SHA256
```

SHA-256 checks integrity, not publisher identity. Download from this repository's
release page. Firmware signatures are not enforced in this testing release.

## First install from stock Panda firmware

The intended stock-preserving route is the **stock web interface's firmware
update function**, uploading only `SnapHeater-U1-0.9.9-ota.bin`.
It uses the matching dual-OTA application layout rather than replacing the
bootloader/partition table. This approach follows the documented
[DragonBreath installation findings](https://github.com/plastikman/DragonBreath/blob/25c831c032d459eb5c573ba6b69022e242ea7bec/README.md#install--update);
the SnapHeater cycle on your exact installed bootloader remains unqualified.

If your stock updater refuses the image, **do not force-write a full image or
erase the device**. Record the stock version and rejection message and ask for help.
Do not assume every stock version accepts custom application updates.

After reboot, use Android's connection wizard to connect to SnapHeater over BLE,
configure/test its Wi-Fi and then configure the U1. Default BLE control PIN in
this tester build is `123456`; it is public and not a strong security boundary.
Keep access on a trusted local network. The REST token is separate and provisioned
through the BLE workflow; do not expose REST to the Internet.

## Updating an existing SnapHeater installation

1. Download the app-only BIN from the release page to the phone.
2. Connect to the correct saved heater **over LAN REST**, not a BLE-only address.
   The app needs the configured REST token and verified device identity.
3. Stop the job. Wait until fan/cooling stops and both fresh valid sensor readings
   are below 30 °C. OTA also rejects active, paused or scheduled work and fault states.
   A warm room can prevent admission: never bypass the checks to update.
4. Open the app's firmware-update card, choose the BIN and confirm the device/update.
5. Keep the connection and power stable. The firmware writes the inactive slot,
   validates size, image structure, project identity and SHA-256, then selects it
   for reboot. Failed/incomplete uploads remain ineligible for boot selection.
6. Reconnect and confirm firmware version **0.9.9** and sane temperatures/status.
   Heating does not start automatically after the update.

The file picker does not automatically download releases. GitHub provides the
OTA asset; the user selects it in the app. OTA over BLE is not implemented.

For API clients: send the raw application bytes to `POST /api/v2/update` with
`X-DragonBreath-Auth`, `Content-Type: application/octet-stream` and
`X-SnapHeater-SHA256` containing the 64-character hex digest. See [API](api.md).
The HTTP connection closes after each request; clients must reconnect.

## Android installation

Open the APK on your phone and allow installation from that source if prompted.
Grant nearby-device/Bluetooth permissions; Android versions may also require
location permissions for discovery. Enable notifications if desired.

With USB debugging, developers can update without deliberately deleting app data:

```powershell
adb install -r SnapHeater-U1-Android-0.9.9-debug.apk
```

A signing-key mismatch can prevent in-place updates. **Do not uninstall blindly**:
uninstalling removes local history and settings. Export data first and report the
mismatch. Version code 3 permits an upgrade from the earlier test app (code 2).

## First supervised session and recovery

Check plausible chamber/PTC readings, printer connection, OFF and cooldown before
running an attended low-target test. Never treat an app animation as proof of
physical fan rotation. Unexpected heat, fan behavior, smells or noise: stop the
test, safely disconnect power when necessary and report the issue. Do not keep
testing damaged hardware or bypass a fault latch.

An interrupted heating session can require **Clear fault / Remove lock** in the
app after the physical cause is resolved and local checks allow it. This does
not restart the job; normal activation is a separate deliberate command.

The inactive slot may initially contain stock, but a later OTA can overwrite it.
Do not promise rollback to stock without checking what is actually in that slot.
Recovery depends on the installed bootloader, not merely application build flags.
See [return-to-stock notes](BACK_TO_ORIGINAL_FW.md). A 4 MB full backup is never
an OTA upload. Do not run generic `idf.py flash` for routine installation.
