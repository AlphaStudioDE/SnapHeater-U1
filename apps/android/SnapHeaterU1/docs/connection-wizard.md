# Connection wizard

1. Connect the phone to SnapHeater over BLE or LAN.
2. Panda Wi-Fi: through BLE, ask Panda to scan (up to 8 visible access points)
   or enter an SSID manually. Enter the password and test on Panda. DHCP success
   precedes saving credentials; candidate settings use WIFI_STORAGE_RAM.
   Failed authentication, missing AP, connection timeout and save failure are
   separate statuses. Failed candidates restore the previous runtime network.
   No MCU restart is required for this Wi-Fi step.
3. After Panda confirms Wi-Fi, discover a printer or enter its Moonraker host/IP
   and port (default 7125). Phone discovery requires the same Wi-Fi subnet.

Wi-Fi setup is available only with heating stopped and the fan off. A policy
reservation prevents starts, overlapping network writes and OTA during testing;
the thermal loop continues. Wi-Fi scan and DHCP waits each have a 15-second
limit, plus driver setup/restore overhead. Android stops its waiting indicator
after 35 seconds without a result and offers reconnect (not a fake password error).
Moonraker host configuration is still loaded at boot: after saving the printer
address, the wizard instructs the user to restart the cooled SnapHeater and
reconnect through step 1. It does not reboot hardware automatically.
The password is masked and is not saved in Android preferences or instance state.

Continue requires fresh printer telemetry confirmed by the firmware. A successful
configuration write is not a successful connection test. Configuration changes
invalidate this confirmation until firmware restart.

Skip opens the application without printer-dependent modes. Auto standby,
Auto standby + Tempering, and the existing printer-dependent Tempering option
remain unavailable. Preheat, Drying, Manual Hold, and OFF do not need printer data.
Modes provides a return to the wizard. Lost/stale printer data also prevents
new printer-dependent activations, without sending an automatic start or stop.

Use matching firmware exposing REST printer.data_ready / BLE pr_ready.
Older firmware without these fields cannot confirm readiness.
The visual preview remains isolated and can show all modes using sample data.
