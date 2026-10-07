# 01 — Concept

## Problem

A room, a lab, a hangar, or a campus bench has a 2.4 GHz habitat that is mostly invisible. Access points advertise. Phones ask for networks they remember. Channels pile up on 1, 6, and 11. None of that requires joining a network to observe, and none of it requires the observer to transmit.

PERCH is that observer, built to live on a Shrike-fi without a host laptop in the loop. Dual Type-C means the board can be powered from a battery bank on one port and still present a console, or stream a census while a second cable is used only for bring-up.

## Intent

An environmental auditor answers three questions:

1. What is advertising? Beacon SSIDs, channels, RSSI, whether the beacon is hidden.
2. How crowded is the band? Management-frame rate per channel, and which channels are quiet.
3. How occupied is the space? A salted count of distinct transmitters, not a list of identities.

Those three are enough for site survey before placing a link, for a teaching lab on 802.11 management, and for a privacy-preserving occupancy hint. They are not enough to follow a person, open a network, or impersonate an access point. The firmware is shaped so that the missing capabilities are absent, not merely undocumented.

## Why this board

Shrike-fi is the only Shrike variant with a radio. The ESP32-S3FN8 brings 2.4 GHz 802.11 b/g/n, BLE 5 (unused here), 512 KB SRAM, and 8 MB in-package flash. The Renesas SLG47910 (1120 5-input LUTs) cannot implement an 802.11 PHY; it is the wrong size for packet parsing. It is the right size for a status stretcher: take a one-bit activity strobe from the MCU and hold the FPGA LED long enough for a human to see a busy channel.

Dual USB-C is used as a split plane. The CH9102 UART port is the debug console. Native USB is the audit stream. Neither port is required for the radio to run; a field unit can log to flash and blink the LED with no host attached.

## Non-goals

- 5 GHz or 6 GHz. The ESP32-S3 radio is 2.4 GHz only.
- BLE survey. A later mode could passive-scan advertisements; v1 does not, so the Wi-Fi callback stays deterministic.
- Data-frame inspection. Payloads, QoS headers, and 4-way handshakes are out of scope.
- FPGA packet path. The 4-bit MCU–FPGA link carries status, not frames.
- A user interface on the board. There is no display. The census is JSON and an LED.
