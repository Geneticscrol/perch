# 05 — Bring-up

## Hardware assumed

- Vicharak Shrike-fi, ESP32-S3FN8, 8 MB in-package flash (Zephyr board definition and the product README).
- MCU user LED on GPIO21, active high.
- UART console on GPIO43 TX / GPIO44 RX via the CH9102 Type-C port.
- Native USB Serial/JTAG on GPIO19 / GPIO20, second Type-C.
- FPGA configuration SPI2: GPIO12 SCLK, GPIO10 SS, GPIO11 MOSI, GPIO13 MISO/CONFIG, GPIO9 EN, GPIO8 PWR. Unused in v1.
- All I/O is 3.3 V. Do not power from USB-C and the 5 V header at the same time.

Vicharak’s hardware overview also describes an external W25Q32 (4 MB) for bitstream and firmware. The ESP32-S3FN8 already has 8 MB in package, and the current board table lists 8 MB for Shrike-fi. Partition the in-package flash. Do not assume the external part is populated until the board in hand is checked.

Optional 8 MB PSRAM (LY68L6400) and the BMS parts are do-not-place on the base board. PERCH does not need either.

## Toolchain

- ESP-IDF 5.2 or later.
- Target `esp32s3`.
- Python 3.10+ on the host, plus `pyserial` for the console.

## Flash layout

`partitions.csv` keeps the app under 2 MB and leaves the rest of the 8 MB empty. No data partition in v1.

| Name | Type | Size |
|---|---|---|
| nvs | data / nvs | 24 KB |
| phy_init | data / phy | 4 KB |
| factory | app / factory | 2 MB |

## Build

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

On Windows the UART bridge usually appears as a COM port. Hold BOOT while plugging in if download mode is needed; `idf.py flash` handles the reset on a working bridge.

Menuconfig defaults live in `sdkconfig.defaults`: 8 MB flash, CDC-capable USB, management-only posture. If the board enumerates with a 4 MB flash definition, stop and check the module marking before changing the size. An oversized flash setting will fail at boot, not silently wrap.

## First run, expected

Console, once per second after the first dwell:

```
I (1234) perch: radio up, promiscuous, mgmt filter, channel 1, tx path absent
I (1534) perch: census ch=1 beacons=3 probes=0 stations=2
```

GPIO21 toggles on each accepted management frame, rate-limited to 10 Hz so a busy channel does not look like a solid lamp. Solid off means the callback is not firing. Solid on means the rate limiter is stuck; that is a bug.

## Antenna and placement

The on-chip RF path is not a survey antenna. Keep the board off a ground plane of aluminium, and do not close a hand over the antenna keep-out. Comparative readings between two spots on the same board are usable. Absolute dBm against a calibrated instrument will not match, and the docs do not claim it will.

## FPGA bitstream

Not required. If the stretcher is built in Go Configure and flashed with ShrikeFlash, drive the activity GPIO documented in `fpga/README.md` before expecting the FPGA LED to track frames. With the FPGA held in reset, only GPIO21 moves.
