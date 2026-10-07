#pragma once

// PERCH — receive-only census. Nothing in this file arms a transmit path.

#define PERCH_FW            "perch-0.1"
#define PERCH_DWELL_MS      300
#define PERCH_REPORT_MS     2000
#define PERCH_CHANNELS      13
#define PERCH_STATION_TTL_MS 60000
#define PERCH_MAX_STATIONS  64
#define PERCH_MAX_SSIDS     24

// 0 = hop 1..PERCH_CHANNELS. 1..13 = park on that channel.
#define PERCH_LOCK_CHANNEL  0

#define PERCH_LED_PIN       21

// Directed probe SSIDs are counted, not stored, unless this is 1.
#define PERCH_LOG_PROBES    0
