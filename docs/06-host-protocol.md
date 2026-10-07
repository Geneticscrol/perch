# 06 — Host protocol

Transport is 115200 8N1 on the UART console, and USB CDC on the native port if `PERCH_USB_CDC` is enabled. Both carry the same lines. Framing is UTF-8 JSON, one object per line, no length prefix.

## Lines

| `t` | When | Fields |
|---|---|---|
| `hello` | once after radio up | `fw`, `board`, `salted` (always true), `channels`, `dwell_ms` |
| `census` | every `PERCH_REPORT_MS` | channel, rates, `ch_counts` (last completed dwell per channel, length 13), SSID list |
| `hop` | on channel change, only if verbose | `ch` |
| `fault` | init failure | `msg` |

Unknown keys must be ignored by the host. The board will add fields; it will not reuse a field for a new meaning.

## Host tool

`host/perch_console.py` opens the serial port, ignores non-JSON lines (IDF log prefixes), and prints a rolling table of SSIDs plus the latest occupancy line. It does not write commands. The protocol is one-way on purpose: a host cannot talk the radio into a different mode.

## Clock

`uptime_s` is milliseconds since boot divided by 1000. There is no wall clock. Correlate with the host’s receive time if a site log needs one.

## Integrity

No signature. This is a lab stream on a short cable. If the stream later leaves the bench, treat it as unauthenticated telemetry and do not feed it to an access decision.
