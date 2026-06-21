# Backup, restore and original flash layout notes

These notes are intended for SnapHeater U1 development and should be adapted to the actual serial port and flash size.

## Original observed flash layout

```csv
# Name, Type, SubType, Offset, Size
nvs,data,nvs,0x9000,0x5000
otadata,data,ota,0xE000,0x2000
app0,app,ota_0,0x10000,0x1E0000
app1,app,ota_1,0x1F0000,0x1E0000
spiffs,data,spiffs,0x3D0000,0x2F000
coredump,data,coredump,0x3FF000,0x1000
```

## Backup before flashing

```bash
esptool.py --chip esp32c3 -p <PORT> -b 460800 read_flash 0x0 0x400000 panda_breath_full_backup.bin
```

## Restore full backup

```bash
esptool.py --chip esp32c3 -p <PORT> -b 460800 write_flash 0x0 panda_breath_full_backup.bin
```

## Generic original-firmware image

This repository includes `firmware/original/generic.bin` as a generic 4 MB restoration image. See [BACK_TO_ORIGINAL_FW.md](BACK_TO_ORIGINAL_FW.md) for the full restore procedure, hash and validation notes.

## Privacy warning

A full flash backup can contain Wi-Fi credentials, device IDs, access codes and other private data. Do not upload full dumps publicly.
