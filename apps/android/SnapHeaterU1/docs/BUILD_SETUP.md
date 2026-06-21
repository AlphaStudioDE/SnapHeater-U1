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

If a Gradle Wrapper is available, the command-line build should be:

```bash
./gradlew assembleDebug
```

On Windows:

```powershell
.\gradlew.bat assembleDebug
```

## Current repository note

The repository currently contains the Gradle project files, but no committed Gradle Wrapper. If the project is opened in Android Studio first, the wrapper can be generated from the IDE or with:

```bash
gradle wrapper
```

After the wrapper exists, commit these files:

```text
gradlew
gradlew.bat
gradle/wrapper/gradle-wrapper.jar
gradle/wrapper/gradle-wrapper.properties
```

## Safety boundary

The app uses mock data. A successful Android build does not mean heater, fan, BLE, Moonraker or safety behavior is validated on real hardware.
