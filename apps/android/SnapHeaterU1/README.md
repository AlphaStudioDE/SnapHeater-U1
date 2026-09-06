# SnapHeater U1 Android — 0.9.9

Native Kotlin/Jetpack Compose companion for SnapHeater firmware on original Panda
Breath electronics. Android 8.0+ (API 26), BLE and authenticated LAN REST.
English, Polish and German resources are included.

The app provides a connection wizard, saved heaters, task-oriented controls,
pause/stop, temperature charts, event history, CSV/report export and LAN OTA.
Printer monitoring and job execution run on Panda, not on the phone.

Download the sideloadable debug/testing APK from
[release v0.9.9](https://github.com/AlphaStudioDE/SnapHeater-U1/releases/tag/v0.9.9).
It is not a production-signed or Play Store build. Keep the installed app's data:
do not uninstall to resolve a signature mismatch without first exporting history.

## Build

Open this directory in Android Studio with JDK 17 and Android SDK 35.

```powershell
.\gradlew.bat assembleDebug testDebugUnitTest lintDebug
```

Normal authenticated mode activation needs no separate manual arm step.
Firmware interlocks remain authoritative. Background notifications are best effort.
See [installation](../../../docs/INSTALL_0.9.9.md) and
[testing risk notice](../../../docs/HARDWARE_LIABILITY_DISCLAIMER.md).

The visual preview is isolated from physical transports. Preview screens are
not evidence of a real connected heater or successful hardware tests.
Public builds default to `ENABLE_VISUAL_PREVIEW=false`: no preview button or entry.
For private screenshots only, build with `-PlocalPreview=true`; the app identifies
itself as `0.9.9-preview`. Never upload that APK to GitHub releases.
The local preview includes a synthetic History chart and timestamped events,
with no database writes, transport or enabled export actions.
