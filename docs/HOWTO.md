# HOWTO — flash PERCH and read a census

This is the practical guide. The design notes (`01`–`06`) explain why the firmware is shaped the way it is.

## 1. What you are about to run

A ShrikeFi that listens. After upload it hops channels 1–13, prints one JSON object every 2 seconds, and blinks GPIO21 when a management frame arrives. It does not join your Wi-Fi. It does not create an access point.

Survey a bench, lab, or home you are allowed to observe.

## 2. Which USB-C port

ShrikeFi has two:

| Port | How it shows up | Use |
|---|---|---|
| CH9102 USB-UART | "USB Single Serial" / CH9102. Windows COMx, Linux `/dev/ttyACM0` or `ttyUSB0` | Flash. Auto-reset usually works |
| Native ESP32-S3 USB | "USB JTAG/serial debug unit" | Serial after a native-USB build. Hold BOOT if upload fails |

Not sure which socket is which? Plug one in and read the device name.

## 3. Arduino IDE

Follow the quick start in the README. The sketch to open is `firmware/Perch/Perch.ino`.

If upload fails with `Failed to connect`:

1. Unplug the cable.
2. Hold **BOOT**.
3. Plug in, still holding BOOT.
4. Release BOOT and upload again.

Serial Monitor must be **115200**. A healthy boot prints:

```
{"t":"hello","fw":"perch-0.1","board":"shrike-fi","salted":true,"channels":13,"dwell_ms":300}
```

Then a `census` line. GPIO21 dark means the callback is not firing: wrong core, radio not started, or you are inside a shielded box.

## 4. PlatformIO

```bash
cd firmware
pio run -e shrikefi -t upload
pio device monitor
```

`shrikefi` is the CH9102 port. `shrikefi_native_usb` is the native port. Flash size in `platformio.ini` is 4 MB so the image boots whether the board enumerates as 4 MB or 8 MB. The sketch is far smaller than either.

## 5. Read it

Serial Monitor is enough. For a table:

```bash
pip install pyserial
python host/perch_console.py --port /dev/ttyACM0
```

For a browser, Chrome or Edge only (Web Serial): open the [companion](https://geneticscrol.github.io/perch/companion/) after Pages has deployed, or serve `companion/` locally. Click **Connect**, pick the ShrikeFi port. The page does not send commands.

## 6. What the numbers mean

| Field | Meaning |
|---|---|
| `ch` | channel the hopper is on right now |
| `mgmt_rate` | management frames per second in the last window |
| `beacons` / `probes` | counts in that window |
| `stations` | distinct salted transmitters seen in the last 60 s |
| `ssids[].bss` | salted hash of the BSSID, not the BSSID |
| `hidden` | beacon had an empty SSID tag. The name is not guessed |

A quiet channel near zero and a known lab SSID on 1, 6, or 11 is a pass. Absolute dBm against a calibrated meter will not match.

## 7. Troubleshooting

| Symptom | Check |
|---|---|
| No serial port | Charge-only cable, or the other Type-C socket |
| Upload fails | Hold BOOT. Drop upload speed to 115200 |
| `hello` then silence | Antenna keep-out covered, or the board is in a metal drawer |
| Census with `stations: 0` in a busy room | Wait one full hop (about 4 s). Confirm the serial baud |
| Sketch won't compile | Arduino-ESP32 3.x. The sketch uses `esp_wifi.h` from that core |

## 8. What this build will not do

It will not deauthenticate, join, scan actively, or print a MAC address. Those are not hidden commands. They are not in the sketch.
