# Bill of materials

v0.1 needs the board and a cable. Nothing else.

| Item | Required | Notes |
|---|---|---|
| Vicharak ShrikeFi | yes | ESP32-S3 + SLG47910. The FPGA is unused unless you load the stretcher |
| USB-C data cable | yes | Charge-only cables enumerate no serial port |
| Laptop | yes | Arduino IDE 2.x, PlatformIO, or ESP-IDF 5.2+ |
| Power bank | no | Either USB-C port can power the board. Do not also feed the 5 V header |
| Phone | no | The census is serial / browser, not a BLE app |

All user I/O is 3.3 V. Do not power from USB-C and the 5 V header at the same time.
