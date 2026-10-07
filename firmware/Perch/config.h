#pragma once

// PERCH — receive-only census. Nothing in this file arms a transmit path.

#define PERCH_FW            "perch-0.2"
#define PERCH_DWELL_MS      300
#define PERCH_REPORT_MS     2000
#define PERCH_CHANNELS      13
#define PERCH_STATION_TTL_MS 60000
#define PERCH_MAX_STATIONS  64
#define PERCH_MAX_SSIDS     24

// 0 = hop 1..PERCH_CHANNELS. 1..13 = park on that channel.
#define PERCH_LOCK_CHANNEL  0

#define PERCH_LED_PIN       21

// Pulse a header GPIO for the FPGA activity stretcher. Off unless wired.
// GPIO14 is not the config SPI bus (that is GPIO8–13). Wire it to an FPGA input.
#define PERCH_FPGA_STROBE   0
#define PERCH_FPGA_STROBE_PIN 14

// Directed probe SSIDs are counted, not stored, unless this is 1.
#define PERCH_LOG_PROBES    0

// Append census lines to LittleFS /census.log. Off by default: no residue,
// and the default partition scheme does not need a filesystem.
// Set to 1, then in platformio.ini add: board_build.filesystem = littlefs
#define PERCH_LOG_TO_FS     0
#define PERCH_LOG_MAX_BYTES 65536
