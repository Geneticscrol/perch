# 03 — Frame model

PERCH looks at 802.11 management frames only. Frame control type is 0. Everything else is dropped on the first half-word.

## Subtypes kept

| Subtype | Value | What it tells the census | Logged content |
|---|---|---|---|
| Beacon | 8 | A BSS is advertising on this channel | SSID (or “hidden”), channel IE if present, HT present, RSSI |
| Probe request | 4 | A station is searching | Count and hashed transmitter. Directed SSID not stored by default. |
| Probe response | 5 | An AP answered a search | Treated as an advertisement, same fields as a beacon |

Association, authentication, action, and ATIM frames are counted in a single “other management” bucket and not parsed. They are rare in a passive dwell and not needed for the habitat questions.

## Header fields used

From the 24-byte management header, when the buffer is long enough:

- Frame control: protocol version must be 0, type must be 0, subtype as above.
- Address 2: transmitter. Hashed. Never copied to the output struct.
- Address 3: BSSID for beacon and probe response. Hashed separately as `bss_hash` so two radios advertising the same SSID can be told apart without keeping the BSSID.

Sequence control is ignored. Duration is ignored. Address 1 is ignored except as a sanity check that the buffer is long enough.

## Information elements

Beacon and probe-response bodies start 12 bytes after the management header (timestamp, beacon interval, capability). PERCH walks tags until SSID (tag 0), DS Parameter Set (tag 3), and HT Capabilities (tag 45) are found or the buffer ends. Walk is bounded by the captured length. A truncated IE ends the walk; it does not get padded or guessed.

SSID rules:

- Tag present, length 0: hidden, reported as an empty SSID with `hidden: true`.
- Tag present, length 1–32: copied, non-printable bytes replaced with `?`.
- Tag absent: omitted, not invented.

DS Parameter Set, when present and length 1, overrides the hopper’s current channel for that record. Access points do not always sit on the channel a passive hopper thinks it is on, because of overlap. The IE is the better channel label.

## RSSI

Taken from `wifi_promiscuous_pkt_t.rx_ctrl.rssi`. Stored as the latest value and as a per-channel EMA with alpha 1/8. No calibration against a reference antenna is claimed. The Shrike-fi uses the on-chip antenna path; absolute dBm is comparative within one board, not a survey-grade measurement.

## What is discarded

- Frame body after the three IEs above.
- All data frames, including those carrying EAPOL.
- All control frames (acks, RTS/CTS, block ack).
- Radiotap-style vendor extensions. The IDF callback already presents `rx_ctrl` plus the 802.11 frame.

## Output object (one census tick)

```json
{
  "t": "census",
  "uptime_s": 42,
  "ch": 6,
  "dwell_ms": 300,
  "mgmt_rate": 18.4,
  "beacons": 11,
  "probes": 4,
  "other_mgmt": 1,
  "stations": 7,
  "ssids": [
    {"ssid": "lab-ap", "ch": 6, "rssi": -47, "hidden": false, "ht": true, "bss": "a1b2c3d4"}
  ]
}
```

`stations` is the count of unexpired hashes. `bss` is the truncated salted hash, not the BSSID. `mgmt_rate` is management frames per second over the last reporting window, not airtime.
