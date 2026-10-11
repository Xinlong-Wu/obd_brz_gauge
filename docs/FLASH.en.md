# Flash & Update

English | [简体中文](FLASH.md)

## Partition table

16 MB flash; the authoritative source is [partitions.csv](../partitions.csv):

| Partition | Offset | Size | Purpose |
|-----------|--------|------|---------|
| nvs | 0x9000 | 24 KB | User settings |
| otadata | 0xF000 | 8 KB | OTA slot selection |
| phy_init | 0x11000 | 4 KB | RF calibration |
| ota_0 | 0x20000 | 3 MB | Firmware slot A |
| ota_1 | 0x320000 | 3 MB | Firmware slot B |
| bootmedia | 0x620000 | 5.875 MB | Boot animation |

> The theme_0 partition (0x620000, 4 MB) from the old partition table was
> removed along with the theme architecture; bootmedia moved up into its
> place. **Devices updated over the air end up with a misplaced boot
> animation and need a USB re-flash of bootmedia.bin** (OTA cannot rewrite
> the partition table itself).

## USB flashing

Build the firmware yourself (see the [README](../README.en.md) quick start);
after `idf.py build`, flash the artifacts from `build/` with
[esptool.py](https://github.com/espressif/esptool) (`pip install esptool`):

```bash
esptool.py --chip esp32s3 -p PORT -b 460800 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0      build/bootloader/bootloader.bin \
  0x8000   build/partition_table/partition-table.bin \
  0xf000   build/ota_data_initial.bin \
  0x20000  build/obd_brz_gauge.bin \
  0x620000 build/bootmedia.bin
```

- `bootmedia.bin` (boot animation) is optional — without it you just don't
  get a boot animation. The image uses the raw layout (generated at build
  time by `tools/gen_bootmedia.py`: manifest text at offset 0, frame data from
  0x1000); it is not a filesystem image

## App OTA upgrade (dual-slot rollback)

The companion phone app (distributed separately, not in this repo) can push firmware and boot
animations. Since the JDM UI rewrite the **on-device OTA entry page is gone**
(the `ota_wifi_server` HTTP backend is kept, but currently unreachable from
the UI; the entry page is to be re-introduced in the JDM design language).
For a firmware update:

1. The app fetches `latest.json` and reads the device manifest over BLE to
   check hardware compatibility; a mismatch aborts the flash
2. The new firmware is written to the **currently inactive OTA slot**
   (`esp_ota_ops`)
3. The device reboots into the new slot; the slot is marked valid only after
   a **15-second boot self-check** — if the new firmware crashes early, the
   bootloader rolls back to the previous slot

Boot animations update over WiFi OTA: the request body is a `[manifest][bin]`
concatenated stream (headers and endpoints in [APP_PROTOCOL.en.md](APP_PROTOCOL.en.md)).
The device first erases and writes the frame-data region at 0x1000 and commits
the **manifest last** — an interrupted transfer or power loss just leaves the
manifest invalid (boot skips the animation) instead of playing a corrupted
half-written one. RS485 acquisition is paused during the transfer to free the CPU.

The full transfer protocol (service UUIDs, HTTP endpoints, headers) is in
[APP_PROTOCOL.en.md](APP_PROTOCOL.en.md).
