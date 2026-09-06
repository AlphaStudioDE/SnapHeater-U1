# SnapHeater U1 — Android gallery

<p align="center"><img src="../apps/android/SnapHeaterU1/app/src/main/res/drawable-nodpi/snapheater_app_icon.png" width="220" alt="SnapHeater U1 Android app icon"></p>
<p align="center"><strong>Your chamber heater. Connected to your print.</strong></p>

Screenshots supplied by the project author for the **0.9.9 development/testing
series**. Tap an image to see the original at full resolution.

**Presentation, not hardware-test evidence:** the dashboard shows illustrative
values; History is from the local `0.9.9-preview` build and uses synthetic data.
The public Android APK has no disconnected-device preview option. The older
dashboard and mode captures show three navigation tabs; the current app also
includes History, as shown in the fourth screenshot. The exact build code of
the earlier captures is not embedded in those images.

## 1. Connect your heater

BLE discovery and authenticated LAN connection, with the runtime-safeguard
notice visible. Address and token fields in this capture are empty.

<a href="assets/gallery/android-connection.jpg"><img src="assets/gallery/android-connection.jpg" width="320" alt="SnapHeater Android connection screen with empty address and token fields"></a>

## 2. See the chamber at a glance

Large chamber reading, requested target, active task and a prominent heating
stop action. The displayed 38 °C / 45 °C values are illustrative, not a reported
measurement from a qualified test. Pause is disabled in this older preview capture.

<a href="assets/gallery/android-dashboard.jpg"><img src="assets/gallery/android-dashboard.jpg" width="320" alt="Illustrative preheat dashboard with chamber, target and Stop control"></a>

## 3. Choose a task, not a collection of sliders

Icon-led cards for preheat, drying, AUTO, AUTO + Tempering, manual hold and
standalone tempering. The AUTO + Tempering card combines the AUTO icon,
a plus sign and the tempering icon.

<a href="assets/gallery/android-modes.jpg"><img src="assets/gallery/android-modes.jpg" width="280" alt="Heating mode cards with dedicated icons"></a>

## 4. Follow the temperature story

Chamber, PTC and requested-target curves, 2 h / 24 h selection and timestamped
warning history. This capture explicitly labels its data as synthetic. Export
buttons are disabled in the local preview; real-device history supports export.

<a href="assets/gallery/android-history-preview.jpg"><img src="assets/gallery/android-history-preview.jpg" width="320" alt="Synthetic temperature history with a preheat-hold-tempering curve and example warnings"></a>

---

[Download the public testing release](https://github.com/AlphaStudioDE/SnapHeater-U1/releases/tag/v0.9.9)
· [Install guide](INSTALL_0.9.9.md) · [Testing risk notice](HARDWARE_LIABILITY_DISCLAIMER.md)
· [Back to the project](../README.md)
