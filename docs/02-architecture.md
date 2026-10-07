# 02 — Architecture

## Planes

```
  air (2.4 GHz, management only)
        |
        v
  ESP32-S3 Wi-Fi baseband
        |
        v
  promiscuous callback  ---- drop if not mgmt, or not a subtype we keep
        |
        v
  RAM census (ring + tables)
        |
        +---- reporter task ---- JSON lines ---- native USB or UART
        |
        +---- GPIO21 activity strobe ---- optional FPGA stretcher ---- FPGA LED
```

Three tasks, one callback.

| Piece | Where it runs | Budget |
|---|---|---|
| RX callback | Wi-Fi task context | Parse header, bump counters, push a small event. No printf. No hash. |
| Census task | Core 0, priority above idle | Fold events, hash addresses, expire stale stations |
| Reporter | Core 0 | Emit one JSON object on a fixed period |
| Hopper | esp_timer | `esp_wifi_set_channel` on dwell expiry |

The callback must return quickly. A full beacon can be long; PERCH copies at most the fixed header plus the first tagged IE span it needs (SSID, DS parameter set, HT capabilities present/absent). Bodies are not queued.

## Radio posture

Startup sequence, and the only legal sequence in this firmware:

1. NVS init, netif init, default event loop.
2. `esp_wifi_init` with the default config.
3. Mode `WIFI_MODE_STA`. This is the IDF-supported way to own the radio without joining.
4. `esp_wifi_start`.
5. Promiscuous filter mask = `WIFI_PROMIS_FILTER_MASK_MGMT`.
6. Register the RX callback, then `esp_wifi_set_promiscuous(true)`.
7. Disable power save. Set protocol to 802.11 b/g/n.
8. Park on channel 1. Arm the hopper.

Explicitly absent from `app_main` and from every other file:

- `esp_wifi_connect`
- `esp_wifi_scan_start` (active scan transmits)
- `esp_wifi_80211_tx` and any vendor TX hook
- `esp_wifi_set_promiscuous_ctrl_filter` left at default so control frames are not delivered

Station mode without connect does not associate. The board does not request an IP. The DHCP client is not started.

## Channel hop

Dwell default is 300 ms on channels 1 through 13, round-robin. Hopping is the right trade for a habitat survey and the wrong trade for following one BSS; PERCH is the former. A compile-time lock (`PERCH_LOCK_CHANNEL`) parks on one channel for a bench measurement.

RSSI samples are stored per channel as an exponential moving average of beacon RSSI, plus a raw management-frame counter for the dwell. Occupancy is frame count, not airtime. The ESP32 promiscuous path does not give a reliable airtime figure without extra timestamp work, and the doc does not pretend it does.

## Identity handling

Each boot draws a 128-bit salt from `esp_fill_random`. A station key is SHA-256(salt || address || salt), truncated to 8 hex characters. The salt never leaves the board. Restarting the board starts a new namespace, so yesterday’s hash cannot be joined to today’s. That is deliberate.

The station table holds hash, last RSSI, last channel, last-seen tick, and a flag for “sent a beacon” versus “sent a probe”. Capacity is 128 entries. On overflow the oldest entry is replaced. Expiry is 60 s without a frame.

## Storage

v1 does not write the census to flash. 8 MB is enough for a later ring log; keeping v1 RAM-only avoids wearing flash during a long dwell and avoids a forensic residue on the board. A future `PERCH_LOG_TO_FS` can append JSON lines to a LittleFS partition. It is specified in the host protocol note and not built.

## FPGA

The SLG47910 is configured over SPI2 (GPIO12 SCLK, GPIO10 SS, GPIO11 MOSI, GPIO13 MISO/CONFIG, GPIO9 EN, GPIO8 PWR). PERCH v1 firmware runs with the FPGA held in reset. The optional bitstream in `fpga/` is a pulse stretcher: a rising edge on an MCU GPIO becomes a ~200 ms high on the FPGA LED. That bitstream is documentation of the split, not a requirement to run the auditor.

## Failure behaviour

If Wi-Fi init fails, GPIO21 stays off and the console prints one error line. The firmware does not fall back to a scan. There is no second mode.
