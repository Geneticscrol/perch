# PERCH firmware — ESP-IDF

Receive-only 802.11 management census for Shrike-fi.

Build from this directory with ESP-IDF 5.2+:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

`sdkconfig.defaults` selects 8 MB flash and the management-only posture. See `../docs/05-bringup.md`.

The application never calls `esp_wifi_connect`, `esp_wifi_scan_start`, or `esp_wifi_80211_tx`.
