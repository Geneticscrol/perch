/*
 * PERCH — passive environmental radio census for Vicharak Shrike-fi.
 *
 * Receive-only. Management frames only. No association, no active scan,
 * no 802.11 transmit path. Station addresses are salted-hashed before
 * they are eligible to appear in a log line.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "mbedtls/sha256.h"

#define PERCH_TAG            "perch"
#define PERCH_LED_GPIO       21
#define PERCH_CHANNELS       13
#define PERCH_DWELL_MS       300
#define PERCH_REPORT_MS      2000
#define PERCH_STATION_TTL_MS 60000
#define PERCH_MAX_STATIONS   128
#define PERCH_MAX_SSIDS      32
#define PERCH_QUEUE_LEN      48
#define PERCH_LOG_PROBES     0

typedef struct {
    uint8_t subtype;
    int8_t rssi;
    uint8_t channel;
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint8_t ssid_len;
    uint8_t hidden;
    uint8_t ht;
    char ssid[33];
} perch_evt_t;

typedef struct {
    char hash[9];
    int8_t rssi;
    uint8_t channel;
    uint8_t from_beacon;
    int64_t last_ms;
    uint8_t used;
} perch_sta_t;

typedef struct {
    char ssid[33];
    char bss[9];
    uint8_t channel;
    int8_t rssi;
    uint8_t hidden;
    uint8_t ht;
    uint8_t used;
} perch_ssid_t;

static QueueHandle_t s_q;
static uint8_t s_salt[16];
static uint8_t s_channel = 1;
static perch_sta_t s_stas[PERCH_MAX_STATIONS];
static perch_ssid_t s_ssids[PERCH_MAX_SSIDS];
static uint32_t s_beacons;
static uint32_t s_probes;
static uint32_t s_other;
static int64_t s_window_start;
static volatile uint32_t s_led_hold;

static void hash8(const uint8_t addr[6], char out[9])
{
    uint8_t dig[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, s_salt, sizeof s_salt);
    mbedtls_sha256_update(&ctx, addr, 6);
    mbedtls_sha256_update(&ctx, s_salt, sizeof s_salt);
    mbedtls_sha256_finish(&ctx, dig);
    mbedtls_sha256_free(&ctx);
    snprintf(out, 9, "%02x%02x%02x%02x", dig[0], dig[1], dig[2], dig[3]);
}

static int ie_walk(const uint8_t *body, int len, perch_evt_t *ev)
{
    int i = 12; /* timestamp(8) + interval(2) + capability(2) */
    int saw_ssid = 0;
    if (len < 12) {
        return 0;
    }
    while (i + 2 <= len) {
        uint8_t id = body[i];
        uint8_t elen = body[i + 1];
        if (i + 2 + elen > len) {
            break;
        }
        const uint8_t *v = body + i + 2;
        if (id == 0 && !saw_ssid) {
            saw_ssid = 1;
            if (elen == 0) {
                ev->hidden = 1;
                ev->ssid_len = 0;
                ev->ssid[0] = 0;
            } else if (elen <= 32) {
                for (uint8_t k = 0; k < elen; k++) {
                    char c = (char)v[k];
                    ev->ssid[k] = (c >= 32 && c < 127) ? c : '?';
                }
                ev->ssid[elen] = 0;
                ev->ssid_len = elen;
            }
        } else if (id == 3 && elen == 1) {
            if (v[0] >= 1 && v[0] <= 13) {
                ev->channel = v[0];
            }
        } else if (id == 45) {
            ev->ht = 1;
        }
        i += 2 + elen;
    }
    return saw_ssid;
}

static void promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT) {
        return;
    }
    const wifi_promiscuous_pkt_t *pp = buf;
    const uint8_t *f = pp->payload;
    /* sig_len includes the 4-byte FCS; do not walk it as an IE. */
    int len = pp->rx_ctrl.sig_len - 4;
    if (len < 24) {
        return;
    }
    uint16_t fc = f[0] | ((uint16_t)f[1] << 8);
    if ((fc & 0x000c) != 0) {
        return; /* not management */
    }
    uint8_t subtype = (fc >> 4) & 0x0f;
    if (subtype != 8 && subtype != 4 && subtype != 5) {
        perch_evt_t drop = {0};
        drop.subtype = subtype;
        drop.rssi = pp->rx_ctrl.rssi;
        drop.channel = s_channel;
        xQueueSend(s_q, &drop, 0);
        return;
    }
    perch_evt_t ev = {0};
    ev.subtype = subtype;
    ev.rssi = pp->rx_ctrl.rssi;
    ev.channel = s_channel;
    memcpy(ev.addr2, f + 10, 6);
    memcpy(ev.addr3, f + 16, 6);
    if (subtype == 8 || subtype == 5) {
        ie_walk(f + 24, len - 24, &ev);
    }
    s_led_hold = 1;
    xQueueSend(s_q, &ev, 0);
}

static void note_station(const perch_evt_t *ev)
{
    char h[9];
    hash8(ev->addr2, h);
    int64_t now = esp_timer_get_time() / 1000;
    int free_ix = -1;
    int oldest = 0;
    for (int i = 0; i < PERCH_MAX_STATIONS; i++) {
        if (!s_stas[i].used) {
            if (free_ix < 0) {
                free_ix = i;
            }
            continue;
        }
        if (strcmp(s_stas[i].hash, h) == 0) {
            s_stas[i].rssi = ev->rssi;
            s_stas[i].channel = ev->channel;
            s_stas[i].last_ms = now;
            if (ev->subtype == 8) {
                s_stas[i].from_beacon = 1;
            }
            return;
        }
        if (s_stas[i].last_ms < s_stas[oldest].last_ms) {
            oldest = i;
        }
    }
    int ix = free_ix >= 0 ? free_ix : oldest;
    memset(&s_stas[ix], 0, sizeof s_stas[ix]);
    memcpy(s_stas[ix].hash, h, 9);
    s_stas[ix].rssi = ev->rssi;
    s_stas[ix].channel = ev->channel;
    s_stas[ix].last_ms = now;
    s_stas[ix].from_beacon = ev->subtype == 8;
    s_stas[ix].used = 1;
}

static void note_ssid(const perch_evt_t *ev)
{
    if (ev->subtype != 8 && ev->subtype != 5) {
        return;
    }
    if (ev->subtype == 5 && !PERCH_LOG_PROBES && ev->ssid_len == 0 && !ev->hidden) {
        return;
    }
    char bss[9];
    hash8(ev->addr3, bss);
    for (int i = 0; i < PERCH_MAX_SSIDS; i++) {
        if (s_ssids[i].used && strcmp(s_ssids[i].bss, bss) == 0) {
            s_ssids[i].rssi = ev->rssi;
            s_ssids[i].channel = ev->channel;
            s_ssids[i].ht = ev->ht;
            return;
        }
    }
    for (int i = 0; i < PERCH_MAX_SSIDS; i++) {
        if (!s_ssids[i].used) {
            memset(&s_ssids[i], 0, sizeof s_ssids[i]);
            memcpy(s_ssids[i].ssid, ev->ssid, sizeof s_ssids[i].ssid);
            memcpy(s_ssids[i].bss, bss, 9);
            s_ssids[i].channel = ev->channel;
            s_ssids[i].rssi = ev->rssi;
            s_ssids[i].hidden = ev->hidden;
            s_ssids[i].ht = ev->ht;
            s_ssids[i].used = 1;
            return;
        }
    }
}

static int live_stations(void)
{
    int64_t now = esp_timer_get_time() / 1000;
    int n = 0;
    for (int i = 0; i < PERCH_MAX_STATIONS; i++) {
        if (!s_stas[i].used) {
            continue;
        }
        if (now - s_stas[i].last_ms > PERCH_STATION_TTL_MS) {
            s_stas[i].used = 0;
            continue;
        }
        n++;
    }
    return n;
}

static void json_escape(const char *in, char *out, size_t out_len)
{
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 2 < out_len; i++) {
        char c = in[i];
        if (c == '"' || c == '\\') {
            out[j++] = '\\';
        }
        out[j++] = c;
    }
    out[j] = 0;
}

static void emit_census(void)
{
    int64_t now = esp_timer_get_time() / 1000;
    double dt = (now - s_window_start) / 1000.0;
    if (dt < 0.2) {
        dt = 0.2;
    }
    uint32_t frames = s_beacons + s_probes + s_other;
    double rate = frames / dt;
    int stas = live_stations();
    printf("{\"t\":\"census\",\"uptime_s\":%lld,\"ch\":%u,\"dwell_ms\":%d,"
           "\"mgmt_rate\":%.1f,\"beacons\":%" PRIu32 ",\"probes\":%" PRIu32
           ",\"other_mgmt\":%" PRIu32 ",\"stations\":%d,\"ssids\":[",
           (long long)(now / 1000), s_channel, PERCH_DWELL_MS, rate,
           s_beacons, s_probes, s_other, stas);
    int first = 1;
    for (int i = 0; i < PERCH_MAX_SSIDS; i++) {
        if (!s_ssids[i].used) {
            continue;
        }
        char esc[80];
        json_escape(s_ssids[i].ssid, esc, sizeof esc);
        printf("%s{\"ssid\":\"%s\",\"ch\":%u,\"rssi\":%d,\"hidden\":%s,\"ht\":%s,\"bss\":\"%s\"}",
               first ? "" : ",", esc, s_ssids[i].channel, s_ssids[i].rssi,
               s_ssids[i].hidden ? "true" : "false",
               s_ssids[i].ht ? "true" : "false", s_ssids[i].bss);
        first = 0;
    }
    printf("]}\n");
    fflush(stdout);
    s_beacons = s_probes = s_other = 0;
    s_window_start = now;
    ESP_LOGI(PERCH_TAG, "census ch=%u stations=%d", s_channel, stas);
}

static void fold_task(void *arg)
{
    perch_evt_t ev;
    int64_t next_hop = esp_timer_get_time() / 1000 + PERCH_DWELL_MS;
    int64_t next_rep = esp_timer_get_time() / 1000 + PERCH_REPORT_MS;
    s_window_start = esp_timer_get_time() / 1000;
    while (1) {
        if (xQueueReceive(s_q, &ev, pdMS_TO_TICKS(50))) {
            if (ev.subtype == 8) {
                s_beacons++;
                note_station(&ev);
                note_ssid(&ev);
            } else if (ev.subtype == 4 || ev.subtype == 5) {
                s_probes++;
                note_station(&ev);
                if (ev.subtype == 5) {
                    note_ssid(&ev);
                }
            } else {
                s_other++;
            }
        }
        int64_t now = esp_timer_get_time() / 1000;
        if (now >= next_hop) {
            s_channel = (s_channel % PERCH_CHANNELS) + 1;
            esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
            next_hop = now + PERCH_DWELL_MS;
        }
        if (now >= next_rep) {
            emit_census();
            next_rep = now + PERCH_REPORT_MS;
        }
        (void)arg;
    }
}

static void led_task(void *arg)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PERCH_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    while (1) {
        if (s_led_hold) {
            s_led_hold = 0;
            gpio_set_level(PERCH_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(30));
            gpio_set_level(PERCH_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(70));
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        (void)arg;
    }
}

static void radio_up(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));

    wifi_promiscuous_filter_t filt = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT
    };
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filt));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(promisc_cb));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    esp_fill_random(s_salt, sizeof s_salt);
    s_q = xQueueCreate(PERCH_QUEUE_LEN, sizeof(perch_evt_t));
    radio_up();
    printf("{\"t\":\"hello\",\"fw\":\"perch-0.1\",\"board\":\"shrike-fi\","
           "\"salted\":true,\"channels\":%d,\"dwell_ms\":%d}\n",
           PERCH_CHANNELS, PERCH_DWELL_MS);
    ESP_LOGI(PERCH_TAG, "radio up, promiscuous, mgmt filter, channel 1, tx path absent");
    xTaskCreate(fold_task, "perch_fold", 6144, NULL, 5, NULL);
    xTaskCreate(led_task, "perch_led", 2048, NULL, 3, NULL);
}
