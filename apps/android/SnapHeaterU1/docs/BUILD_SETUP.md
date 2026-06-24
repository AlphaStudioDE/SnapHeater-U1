# Android build setup

The Android app is located here:

```text
apps/android/SnapHeaterU1
```

Current app status: Jetpack Compose UI prototype with mock SnapHeater U1 data. BLE integration is intentionally pending until the firmware status/control payloads are validated.

## Required tools

- Android Studio with Android Gradle Plugin support.
- JDK 17 or newer.
- Android SDK with API 35 installed.

## First local build

Open `apps/android/SnapHeaterU1` in Android Studio and let Gradle sync.

The project includes a Gradle Wrapper. The command-line build is:

```bash
./gradlew assembleDebug
```

On Windows:

```powershell
.\gradlew.bat assembleDebug
```

## Local SDK note

Do not commit `local.properties`. It should point to the SDK location on the current machine only.

## Safety boundary

The app uses mock data. A successful Android build does not mean heater, fan, BLE, Moonraker or safety behavior is validated on real hardware.
