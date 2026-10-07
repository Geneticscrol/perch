# 04 — Operating constraints

This note is part of the design. It is not legal advice. Radio and interception rules differ by country, and an operator is responsible for the site they survey.

## Authorized use

PERCH is for a place the operator is allowed to observe: their own bench, lab, home, or a site survey done with the occupant’s permission. The useful outputs are channel occupancy, advertising SSIDs, and a salted station count.

Broadcast beacons are transmitted so that stations can find the network. Hearing them is the ordinary behaviour of any Wi-Fi device. That fact does not authorize recording people, mapping who is in a building, or feeding an address into another system.

## Hard rules in this repository

1. Receive only. No transmit API is linked into the application. A review of the firmware should be able to grep for `esp_wifi_80211_tx`, `esp_wifi_connect`, and `esp_wifi_scan_start` and find nothing.
2. Management frames only. The promiscuous filter is set to the management mask and is not widened at runtime.
3. No raw addresses on the export path. The host protocol has no field for a MAC. A debug compile flag that prints raw addresses does not exist.
4. Directed probe SSIDs stay on the board as a counter. They often contain a network name a phone used to join, which is more sensitive than a beacon. `PERCH_LOG_PROBES` defaults off.
5. No persistent identity. The salt dies with the power cycle. There is no RTC-backed station database.
6. No payload, no handshake, no credential material, by construction rather than by policy.

## India context

The Information Technology Act, 2000 penalizes unauthorized access to a computer system and unauthorized interception of communications. A passive census of broadcast management frames on premises you control is a different act from joining a network, capturing a handshake, or monitoring a person’s traffic. Do not cross that line with this board. If a deployment is for a client site, get the scope in writing before the radio is armed.

## Out of scope, on purpose

The following are not missing features. They are refused features.

- Deauthentication or disassociation, for “testing” or otherwise.
- Evil-twin or soft-AP operation.
- Handshake capture and offline guessing.
- Following a hashed station across days, or across boards (salts are not shared).
- Using probe requests to infer a person’s home network while they walk past.

If a task needs any of those, it needs a different tool, a different authorization, and it will not be added here.
