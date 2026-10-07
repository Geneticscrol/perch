/*
 * PERCH — receive-only 802.11 management census for Vicharak ShrikeFi.
 * Open this folder in Arduino IDE. Settings in config.h.
 *
 * Does not call WiFi.scanNetworks(), WiFi.begin(), or esp_wifi_80211_tx().
 */

#include <WiFi.h>
#include "esp_wifi.h"
#include "mbedtls/sha256.h"
#include "config.h"

struct Sta {
  char hash[9];
  int8_t rssi;
  uint8_t ch;
  uint32_t lastMs;
  bool used;
};

struct SsidRow {
  char ssid[33];
  char bss[9];
  uint8_t ch;
  int8_t rssi;
  bool hidden;
  bool ht;
  bool used;
};

static uint8_t salt[16];
static uint8_t channel = 1;
static Sta stas[PERCH_MAX_STATIONS];
static SsidRow ssids[PERCH_MAX_SSIDS];
static volatile uint32_t beacons, probes, otherMgmt;
static uint32_t windowStart;
static uint32_t nextHop;
static uint32_t nextReport;
static volatile bool ledHold;

static void hash8(const uint8_t *addr, char out[9]) {
  uint8_t dig[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, salt, sizeof salt);
  mbedtls_sha256_update(&ctx, addr, 6);
  mbedtls_sha256_update(&ctx, salt, sizeof salt);
  mbedtls_sha256_finish(&ctx, dig);
  mbedtls_sha256_free(&ctx);
  snprintf(out, 9, "%02x%02x%02x%02x", dig[0], dig[1], dig[2], dig[3]);
}

static void walkIes(const uint8_t *body, int len, char *ssid, uint8_t *ssidLen,
                     bool *hidden, uint8_t *ch, bool *ht) {
  int i = 12;
  bool saw = false;
  if (len < 12) return;
  while (i + 2 <= len) {
    uint8_t id = body[i];
    uint8_t elen = body[i + 1];
    if (i + 2 + elen > len) break;
    const uint8_t *v = body + i + 2;
    if (id == 0 && !saw) {
      saw = true;
      if (elen == 0) {
        *hidden = true;
        *ssidLen = 0;
        ssid[0] = 0;
      } else if (elen <= 32) {
        for (uint8_t k = 0; k < elen; k++) {
          char c = (char)v[k];
          ssid[k] = (c >= 32 && c < 127) ? c : '?';
        }
        ssid[elen] = 0;
        *ssidLen = elen;
      }
    } else if (id == 3 && elen == 1 && v[0] >= 1 && v[0] <= 13) {
      *ch = v[0];
    } else if (id == 45) {
      *ht = true;
    }
    i += 2 + elen;
  }
}

static void noteSta(const uint8_t *addr, int8_t rssi, uint8_t ch) {
  char h[9];
  hash8(addr, h);
  uint32_t now = millis();
  int freeIx = -1;
  int oldest = 0;
  for (int i = 0; i < PERCH_MAX_STATIONS; i++) {
    if (!stas[i].used) {
      if (freeIx < 0) freeIx = i;
      continue;
    }
    if (strcmp(stas[i].hash, h) == 0) {
      stas[i].rssi = rssi;
      stas[i].ch = ch;
      stas[i].lastMs = now;
      return;
    }
    if (stas[i].lastMs < stas[oldest].lastMs) oldest = i;
  }
  int ix = freeIx >= 0 ? freeIx : oldest;
  memset(&stas[ix], 0, sizeof stas[ix]);
  memcpy(stas[ix].hash, h, 9);
  stas[ix].rssi = rssi;
  stas[ix].ch = ch;
  stas[ix].lastMs = now;
  stas[ix].used = true;
}

static void noteSsid(const char *ssid, const uint8_t *bssid, uint8_t ch,
                     int8_t rssi, bool hidden, bool ht) {
  char bss[9];
  hash8(bssid, bss);
  for (int i = 0; i < PERCH_MAX_SSIDS; i++) {
    if (ssids[i].used && strcmp(ssids[i].bss, bss) == 0) {
      ssids[i].rssi = rssi;
      ssids[i].ch = ch;
      ssids[i].ht = ht;
      return;
    }
  }
  for (int i = 0; i < PERCH_MAX_SSIDS; i++) {
    if (!ssids[i].used) {
      memset(&ssids[i], 0, sizeof ssids[i]);
      strncpy(ssids[i].ssid, ssid, 32);
      memcpy(ssids[i].bss, bss, 9);
      ssids[i].ch = ch;
      ssids[i].rssi = rssi;
      ssids[i].hidden = hidden;
      ssids[i].ht = ht;
      ssids[i].used = true;
      return;
    }
  }
}

static void promiscCb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t *pp = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *f = pp->payload;
  int len = pp->rx_ctrl.sig_len - 4;
  if (len < 24) return;
  uint16_t fc = f[0] | ((uint16_t)f[1] << 8);
  if ((fc & 0x000c) != 0) return;
  uint8_t subtype = (fc >> 4) & 0x0f;
  ledHold = true;
  if (subtype != 8 && subtype != 4 && subtype != 5) {
    otherMgmt++;
    return;
  }
  uint8_t ch = channel;
  char ssid[33] = {0};
  uint8_t ssidLen = 0;
  bool hidden = false;
  bool ht = false;
  if (subtype == 8 || subtype == 5) {
    walkIes(f + 24, len - 24, ssid, &ssidLen, &hidden, &ch, &ht);
  }
  if (subtype == 8) beacons++;
  else probes++;
  noteSta(f + 10, pp->rx_ctrl.rssi, ch);
  if (subtype == 8 || (subtype == 5 && (PERCH_LOG_PROBES || ssidLen || hidden))) {
    noteSsid(ssid, f + 16, ch, pp->rx_ctrl.rssi, hidden, ht);
  }
}

static int liveStations() {
  uint32_t now = millis();
  int n = 0;
  for (int i = 0; i < PERCH_MAX_STATIONS; i++) {
    if (!stas[i].used) continue;
    if (now - stas[i].lastMs > PERCH_STATION_TTL_MS) {
      stas[i].used = false;
      continue;
    }
    n++;
  }
  return n;
}

static void emitCensus() {
  uint32_t now = millis();
  float dt = (now - windowStart) / 1000.0f;
  if (dt < 0.2f) dt = 0.2f;
  float rate = (beacons + probes + otherMgmt) / dt;
  Serial.printf("{\"t\":\"census\",\"uptime_s\":%lu,\"ch\":%u,\"dwell_ms\":%d,"
                "\"mgmt_rate\":%.1f,\"beacons\":%lu,\"probes\":%lu,"
                "\"other_mgmt\":%lu,\"stations\":%d,\"ssids\":[",
                now / 1000, channel, PERCH_DWELL_MS, rate,
                (unsigned long)beacons, (unsigned long)probes,
                (unsigned long)otherMgmt, liveStations());
  bool first = true;
  for (int i = 0; i < PERCH_MAX_SSIDS; i++) {
    if (!ssids[i].used) continue;
    Serial.printf("%s{\"ssid\":\"%s\",\"ch\":%u,\"rssi\":%d,\"hidden\":%s,\"ht\":%s,\"bss\":\"%s\"}",
                  first ? "" : ",", ssids[i].ssid, ssids[i].ch, ssids[i].rssi,
                  ssids[i].hidden ? "true" : "false",
                  ssids[i].ht ? "true" : "false", ssids[i].bss);
    first = false;
  }
  Serial.println("]}");
  beacons = probes = otherMgmt = 0;
  windowStart = now;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(PERCH_LED_PIN, OUTPUT);
  for (int i = 0; i < 16; i++) salt[i] = (uint8_t)esp_random();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(100);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  wifi_promiscuous_filter_t filt = {};
  filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&promiscCb);
  esp_wifi_set_promiscuous(true);
  channel = PERCH_LOCK_CHANNEL ? PERCH_LOCK_CHANNEL : 1;
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  windowStart = nextHop = nextReport = millis();
  Serial.printf("{\"t\":\"hello\",\"fw\":\"%s\",\"board\":\"shrike-fi\",\"salted\":true,"
                "\"channels\":%d,\"dwell_ms\":%d}\n",
                PERCH_FW, PERCH_CHANNELS, PERCH_DWELL_MS);
}

void loop() {
  uint32_t now = millis();
  if (!PERCH_LOCK_CHANNEL && now - nextHop >= PERCH_DWELL_MS) {
    channel = (channel % PERCH_CHANNELS) + 1;
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    nextHop = now;
  }
  if (now - nextReport >= PERCH_REPORT_MS) {
    emitCensus();
    nextReport = now;
  }
  if (ledHold) {
    ledHold = false;
    digitalWrite(PERCH_LED_PIN, HIGH);
    delay(20);
    digitalWrite(PERCH_LED_PIN, LOW);
  }
}
