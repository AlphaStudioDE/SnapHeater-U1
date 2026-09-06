/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "moonraker_client.h"
#include "ws_message_buffer.h"
#include "json_guard.h"
#include "app_config.h"
#include "app_state.h"
#include "settings_store.h"
#include "event_log.h"
#include "printer_setup_rules.h"
#include "symbiont_engine.h"
#include "control_lease.h"
#include "safety_latch.h"
#include "freertos/queue.h"
#include <stdatomic.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *TAG = "shu1_moonraker";
static shu1_ws_buffer_t g_rx;

static esp_websocket_client_handle_t g_client = NULL;
static shu1_device_config_t g_devcfg;
typedef struct {shu1_device_config_t cfg;char id[33];} setup_request_t;
static QueueHandle_t g_setup_queue;
static portMUX_TYPE g_setup_mux=portMUX_INITIALIZER_UNLOCKED;
static char g_setup_id[33],g_setup_phase[24]="idle";
static bool g_key_set;
static atomic_bool g_setup_ws_valid;
static portMUX_TYPE g_symbiont_mux=portMUX_INITIALIZER_UNLOCKED;
static shu1_symbiont_engine_t g_symbiont;
static bool g_symbiont_aux, g_symbiont_top;
static const char *CONTROL_QUERY="{\"jsonrpc\":\"2.0\",\"id\":103,\"method\":\"printer.objects.query\",\"params\":{\"objects\":{\"print_stats\":[\"state\"],\"heater_bed\":[\"temperature\",\"target\"],\"webhooks\":[\"state\"]}}}";

static void setup_phase(const char *phase) {
    portENTER_CRITICAL(&g_setup_mux);
    snprintf(g_setup_phase,sizeof(g_setup_phase),"%s",phase);
    portEXIT_CRITICAL(&g_setup_mux);
}
bool shu1_moonraker_setup_status(cJSON *root) {
    char id[33],phase[24];bool key;
    portENTER_CRITICAL(&g_setup_mux);
    memcpy(id,g_setup_id,sizeof(id));memcpy(phase,g_setup_phase,sizeof(phase));key=g_key_set;
    portEXIT_CRITICAL(&g_setup_mux);
    cJSON *status=cJSON_AddObjectToObject(root,"printer_setup");
    bool ok = status && cJSON_AddStringToObject(status,"id",id) &&
        cJSON_AddStringToObject(status,"phase",phase) && cJSON_AddBoolToObject(status,"key_set",key);
    portENTER_CRITICAL(&g_symbiont_mux);
    const char *vent_status=g_symbiont.status ? g_symbiont.status:"auto_or_idle";
    portEXIT_CRITICAL(&g_symbiont_mux);
    return ok && cJSON_AddStringToObject(root,"ventilation_status",vent_status);
}
esp_err_t shu1_moonraker_setup_request(const cJSON *root) {
    setup_request_t request;
    if(!g_setup_queue || !shu1_printer_setup_parse(root,&request.cfg,request.id)) return ESP_ERR_INVALID_ARG;
    shu1_control_snapshot_t ctl;shu1_control_snapshot(&ctl);
    const cJSON *rev=cJSON_GetObjectItemCaseSensitive(root,"expected_revision");
    if(!cJSON_IsNumber(rev) || rev->valuedouble!=(double)ctl.revision ||
       !shu1_control_network_setup_begin()) {memset(&request,0,sizeof(request));return ESP_ERR_INVALID_STATE;}
    portENTER_CRITICAL(&g_setup_mux);
    memcpy(g_setup_id,request.id,sizeof(g_setup_id));
    strcpy(g_setup_phase,"testing");
    portEXIT_CRITICAL(&g_setup_mux);
    if(xQueueSend(g_setup_queue,&request,0)!=pdTRUE) {
        memset(&request,0,sizeof(request));setup_phase("busy");shu1_control_maintenance_end();return ESP_ERR_INVALID_STATE;
    }
    memset(&request,0,sizeof(request));
    shu1_control_release_any();
    return ESP_OK;
}
static int g_rpc_id = 2000;
static int64_t g_last_connect_try_ms = 0;
static int64_t g_last_autodetect_try_ms = 0;
static bool g_subscribe_pending = false;
static bool g_autodetect_pending = false;
static bool g_autodetect_done = false;
static bool g_has_tool_temp_subscription = false;
static bool g_has_chamber_subscription = false;

static char g_chamber_object[64] = "temperature_sensor cavity";
static char g_cavity_fan_object[64] = "";

static int64_t now_ms(void) {
    return esp_timer_get_time() / 1000;
}

static void safe_copy(char *dst, size_t dst_len, const char *src) {
    if (!dst || dst_len == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_len, "%s", src);
}

static void lower_copy(char *dst, size_t dst_len, const char *src) {
    if (!dst || dst_len == 0) return;
    if (!src) src = "";
    size_t i = 0;
    for (; i + 1 < dst_len && src[i]; ++i) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = 0;
}

static bool contains_icase(const char *haystack, const char *needle) {
    if (!haystack || !needle || !needle[0]) return false;
    char h[128];
    char n[64];
    lower_copy(h, sizeof(h), haystack);
    lower_copy(n, sizeof(n), needle);
    return strstr(h, n) != NULL;
}

static const char *normalize_state(const char *state) {
    if (!state || !state[0]) return "idle";
    if (!strcasecmp(state, "standby") || !strcasecmp(state, "ready") ||
        !strcasecmp(state, "operational") || !strcasecmp(state, "idle")) return "idle";
    if (!strcasecmp(state, "printing") || !strcasecmp(state, "print") || !strcasecmp(state, "busy")) return "printing";
    if (!strcasecmp(state, "paused") || !strcasecmp(state, "pause")) return "paused";
    if (!strcasecmp(state, "complete") || !strcasecmp(state, "completed") ||
        !strcasecmp(state, "finished") || !strcasecmp(state, "finish")) return "complete";
    if (!strcasecmp(state, "error") || !strcasecmp(state, "cancelled") || !strcasecmp(state, "canceled")) return "error";
    if (!strcasecmp(state, "timeout")) return "timeout";
    return state;
}

static int tool_index_from_object(const char *object) {
    if (!object || !object[0] || strcmp(object, "extruder") == 0) return 0;
    if (strcmp(object, "extruder1") == 0) return 1;
    if (strcmp(object, "extruder2") == 0) return 2;
    if (strcmp(object, "extruder3") == 0) return 3;
    return 0;
}

static const char *tool_object_from_index(int idx) {
    if (idx == 1) return "extruder1";
    if (idx == 2) return "extruder2";
    if (idx == 3) return "extruder3";
    return "extruder";
}

static void mark_online(bool online) {
    shu1_printer_state_t pr = shu1_state_get_printer();
    bool changed = (pr.moonraker_connected != online);
    pr.moonraker_connected = online;
    if (!online) {

        pr.subscribed = false;
        pr.klippy_ready = false;
        pr.last_update_ms = 0;
    }
    shu1_state_update_printer(&pr);
    if (changed) {
        shu1_event_log_add(online ? "info" : "warn", online ? "moonraker_online" : "moonraker_offline", online ? "Moonraker connected" : "Moonraker disconnected");
    }
}

static void send_json(const char *json) {
    if (!g_client || !esp_websocket_client_is_connected(g_client) || !json) return;
    esp_websocket_client_send_text(g_client, json, strlen(json), pdMS_TO_TICKS(1000));
}

static void send_identify(void) {
    char msg[384];
    snprintf(msg, sizeof(msg),
             "{\"jsonrpc\":\"2.0\",\"method\":\"server.connection.identify\",\"params\":{"
             "\"client_name\":\"SnapHeater U1\",\"version\":\"%s\",\"type\":\"display\",\"url\":\"%s\"},\"id\":1}",
             SHU1_FW_VERSION, SHU1_PROJECT_URL);
    send_json(msg);
}

static void send_server_info(void) {
    send_json("{\"jsonrpc\":\"2.0\",\"method\":\"server.info\",\"id\":100}");
}

static bool object_array_contains(cJSON *arr, const char *name) {
    if (!cJSON_IsArray(arr) || !name || !name[0]) return false;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, arr) {
        if (cJSON_IsString(it) && it->valuestring && strcmp(it->valuestring, name) == 0) return true;
    }
    return false;
}

static bool object_looks_like_temp_sensor(const char *name) {
    return contains_icase(name, "temperature_sensor") ||
           contains_icase(name, "temperature_fan") ||
           contains_icase(name, "bme280") || contains_icase(name, "sht") ||
           contains_icase(name, "htu") || contains_icase(name, "aht") ||
           contains_icase(name, "bmp") || contains_icase(name, "dht") ||
           contains_icase(name, "thermistor");
}

static bool object_preferred_chamber_sensor(const char *name) {
    if (!name) return false;
    char low[128];
    lower_copy(low, sizeof(low), name);
    return strcmp(low, "temperature_sensor cavity") == 0 ||
           strcmp(low, "temperature_sensor chamber") == 0 ||
           strcmp(low, "temperature_sensor enclosure") == 0 ||
           strcmp(low, "temperature_sensor chamber_temp") == 0 ||
           strcmp(low, "temperature_sensor enclosure_temp") == 0;
}

static bool object_matches_chamber_alias(const char *name) {
    return contains_icase(name, "cavity") || contains_icase(name, "chamber") ||
           contains_icase(name, "chamber_temp") || contains_icase(name, "chambertemp") ||
           contains_icase(name, "enclosure") || contains_icase(name, "enclosure_temp") ||
           contains_icase(name, "enclosuretemp") || contains_icase(name, "cabinet") ||
           contains_icase(name, "ambient") || contains_icase(name, "inside");
}

static bool object_matches_cavity_fan(const char *name) {
    if (!name) return false;
    char low[128];
    lower_copy(low, sizeof(low), name);
    return strcmp(low, "fan_generic cavity_fan") == 0 ||
           strcmp(low, "temperature_fan cavity_fan") == 0 ||
           strstr(low, "cavity_fan") != NULL ||
           ((strstr(low, "fan_generic") || strstr(low, "temperature_fan")) && strstr(low, "cavity"));
}

static void autodetect_from_object_array(cJSON *arr) {
    char chamber[64] = "";
    char cavity_fan[64] = "";

    if (cJSON_IsArray(arr)) {
        cJSON *it = NULL;
        cJSON_ArrayForEach(it, arr) {
            if (!cJSON_IsString(it) || !it->valuestring) continue;
            const char *obj = it->valuestring;
            if (object_matches_cavity_fan(obj)) {
                if (!cavity_fan[0] || !strcasecmp(obj, "fan_generic cavity_fan")) safe_copy(cavity_fan, sizeof(cavity_fan), obj);
            }
            if (object_looks_like_temp_sensor(obj) && object_preferred_chamber_sensor(obj)) {
                if (!chamber[0] || !strcasecmp(obj, "temperature_sensor cavity")) safe_copy(chamber, sizeof(chamber), obj);
                continue;
            }
            if (!chamber[0] && object_looks_like_temp_sensor(obj) && object_matches_chamber_alias(obj)) {
                safe_copy(chamber, sizeof(chamber), obj);
            }
        }
    }

    if (!chamber[0]) safe_copy(chamber, sizeof(chamber), "temperature_sensor cavity");
    safe_copy(g_chamber_object, sizeof(g_chamber_object), chamber);
    safe_copy(g_cavity_fan_object, sizeof(g_cavity_fan_object), cavity_fan);

    shu1_printer_state_t pr = shu1_state_get_printer();
    safe_copy(pr.u1_chamber_object, sizeof(pr.u1_chamber_object), g_chamber_object);
    safe_copy(pr.cavity_fan_object, sizeof(pr.cavity_fan_object), g_cavity_fan_object);
    pr.autodetect_done = true;
    pr.last_autodetect_ms = now_ms();
    shu1_state_update_printer(&pr);

    ESP_LOGI(TAG, "U1 object autodetect: chamber='%s', cavity_fan='%s'", g_chamber_object, g_cavity_fan_object[0] ? g_cavity_fan_object : "-");
}

static char *http_get_for(const shu1_device_config_t *device, const char *path, int timeout_ms, int *status_code) {
    if(status_code) *status_code=0;
    if (!path || !device->moonraker_host[0]) return NULL;
    char url[192];
    snprintf(url, sizeof(url), "http://%s:%d%s", device->moonraker_host, device->moonraker_port, path);

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = timeout_ms,
        .buffer_size = 1024,
        .disable_auto_redirect = true, // Never forward the API key to a redirect target.
    };
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h) return NULL;
    if(device->moonraker_api_key[0] && esp_http_client_set_header(h,"X-Api-Key",device->moonraker_api_key)!=ESP_OK) {
        esp_http_client_cleanup(h);return NULL;
    }

    esp_err_t err = esp_http_client_open(h, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(h);
        return NULL;
    }

    int status = esp_http_client_fetch_headers(h);
    (void)status;
    int code = esp_http_client_get_status_code(h);
    if(status_code) *status_code=code;
    if (code != 200) {
        esp_http_client_close(h);
        esp_http_client_cleanup(h);
        return NULL;
    }

    char *buf = calloc(1, SHU1_MOONRAKER_OBJECT_LIST_BUF + 1);
    if (!buf) {
        esp_http_client_close(h);
        esp_http_client_cleanup(h);
        return NULL;
    }
    int total = 0;
    const int64_t deadline=esp_timer_get_time()+6000000;
    while (total < SHU1_MOONRAKER_OBJECT_LIST_BUF) {
        if(esp_timer_get_time()>deadline) break;
        int r = esp_http_client_read(h, buf + total, SHU1_MOONRAKER_OBJECT_LIST_BUF - total);
        if (r <= 0) break;
        total += r;
    }
    buf[total] = 0;
    bool complete=esp_http_client_is_complete_data_received(h);
    esp_http_client_close(h);
    esp_http_client_cleanup(h);
    if(!complete) {free(buf);return NULL;}
    return buf;
}
static char *http_get_alloc(const char *path, int timeout_ms) {return http_get_for(&g_devcfg,path,timeout_ms,NULL);}

static cJSON *fetch_object_list(void) {
    char *payload = http_get_alloc("/printer/objects/list", SHU1_MOONRAKER_HTTP_TIMEOUT_MS);
    if (!payload) return NULL;
    cJSON *root = shu1_json_parse(payload);
    free(payload);
    if (!root) return NULL;
    return root;
}

static void append_obj(char *buf, size_t len, bool *first, const char *obj, const char *fields) {
    if (!buf || !first || !obj || !obj[0] || !fields) return;
    size_t used = strlen(buf);
    snprintf(buf + used, len - used, "%s\"%s\":%s", *first ? "" : ",", obj, fields);
    *first = false;
}

static void send_subscription(bool autodetect) {
    char msg[3072];
    snprintf(msg, sizeof(msg), "{\"jsonrpc\":\"2.0\",\"method\":\"printer.objects.subscribe\",\"params\":{\"objects\":{");
    bool first = true;

    // coroNET-tested Snapmaker U1 set: display_status progress is used instead of relying only on virtual_sdcard.
    append_obj(msg, sizeof(msg), &first, "webhooks", "[\"state\"]");
    append_obj(msg, sizeof(msg), &first, "print_stats", "[\"state\",\"filename\",\"print_duration\",\"total_duration\"]");
    append_obj(msg, sizeof(msg), &first, "display_status", "[\"progress\"]");
    append_obj(msg, sizeof(msg), &first, "virtual_sdcard", "[\"progress\"]");
    append_obj(msg, sizeof(msg), &first, "toolhead", "[\"extruder\"]");
    append_obj(msg, sizeof(msg), &first, "heater_bed", "[\"temperature\",\"target\"]");
    append_obj(msg, sizeof(msg), &first, "print_task_config", "[\"filament_color_rgba\",\"filament_type\"]");

    append_obj(msg, sizeof(msg), &first, "extruder", "[\"temperature\",\"target\"]");
    if (autodetect || g_has_tool_temp_subscription) {
        append_obj(msg, sizeof(msg), &first, "extruder1", "[\"temperature\",\"target\"]");
        append_obj(msg, sizeof(msg), &first, "extruder2", "[\"temperature\",\"target\"]");
        append_obj(msg, sizeof(msg), &first, "extruder3", "[\"temperature\",\"target\"]");
    }
    if (g_has_chamber_subscription && g_chamber_object[0]) append_obj(msg, sizeof(msg), &first, g_chamber_object, "[\"temperature\"]");
    if (g_cavity_fan_object[0]) append_obj(msg, sizeof(msg), &first, g_cavity_fan_object, "[\"speed\"]");
    if (g_symbiont_top) append_obj(msg, sizeof(msg), &first, "purifier", "[\"power_detected\",\"critical_temp_reported\",\"fan_fault_reported\",\"exhaust_fan\"]");

    size_t used = strlen(msg);
    snprintf(msg + used, sizeof(msg) - used, "}},\"id\":%d}", autodetect ? 102 : 101);

    g_subscribe_pending = true;
    send_json(msg);
    ESP_LOGI(TAG, "Moonraker subscribe sent (%s)", autodetect ? "autodetect" : "basic");
}

static void moonraker_autodetect_and_subscribe(void) {
    int64_t now = now_ms();
    if (now - g_last_autodetect_try_ms < SHU1_MOONRAKER_AUTODETECT_RETRY_MS) return;
    g_last_autodetect_try_ms = now;

    cJSON *root = fetch_object_list();
    if (!root) {
        ESP_LOGW(TAG, "object list failed; falling back to basic U1 subscription");
        if (!g_subscribe_pending) send_subscription(false);
        return;
    }

    cJSON *arr = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "result"), "objects");
    g_symbiont_aux=object_array_contains(arr,"fan_generic cavity_fan");
    g_symbiont_top=object_array_contains(arr,"purifier");
    autodetect_from_object_array(arr);

    g_has_tool_temp_subscription = object_array_contains(arr, "extruder") || object_array_contains(arr, "extruder1") ||
                                   object_array_contains(arr, "extruder2") || object_array_contains(arr, "extruder3");
    g_has_chamber_subscription = object_array_contains(arr, g_chamber_object);
    g_autodetect_done = true;
    g_autodetect_pending = false;
    send_subscription(true);
    cJSON_Delete(root);
}

static void update_active_tool_from_status(shu1_printer_state_t *pr, cJSON *status) {
    cJSON *toolhead = cJSON_GetObjectItem(status, "toolhead");
    if (cJSON_IsObject(toolhead)) {
        cJSON *extruder = cJSON_GetObjectItem(toolhead, "extruder");
        if (cJSON_IsString(extruder) && extruder->valuestring) {
            safe_copy(pr->active_tool_object, sizeof(pr->active_tool_object), extruder->valuestring);
            pr->active_tool = tool_index_from_object(extruder->valuestring);
        }
    }
    if (!pr->active_tool_object[0]) safe_copy(pr->active_tool_object, sizeof(pr->active_tool_object), tool_object_from_index(pr->active_tool));
}

static void update_tool_temp_from_status(shu1_printer_state_t *pr, cJSON *status) {
    const char *active = pr->active_tool_object[0] ? pr->active_tool_object : "extruder";
    cJSON *obj = cJSON_GetObjectItem(status, active);
    if (cJSON_IsObject(obj)) {
        cJSON *temperature = cJSON_GetObjectItem(obj, "temperature");
        cJSON *target = cJSON_GetObjectItem(obj, "target");
        if (cJSON_IsNumber(temperature)) pr->active_tool_temp = (float)temperature->valuedouble;
        if (pr->active_tool == 0) {
            if (cJSON_IsNumber(temperature)) pr->extruder_temp = (float)temperature->valuedouble;
            if (cJSON_IsNumber(target)) pr->extruder_target = (float)target->valuedouble;
        }
    }
    cJSON *ext0 = cJSON_GetObjectItem(status, "extruder");
    if (cJSON_IsObject(ext0)) {
        cJSON *temperature = cJSON_GetObjectItem(ext0, "temperature");
        cJSON *target = cJSON_GetObjectItem(ext0, "target");
        if (cJSON_IsNumber(temperature)) pr->extruder_temp = (float)temperature->valuedouble;
        if (cJSON_IsNumber(target)) pr->extruder_target = (float)target->valuedouble;
    }
}

static void update_print_task_config(shu1_printer_state_t *pr, cJSON *status) {
    cJSON *ptc = cJSON_GetObjectItem(status, "print_task_config");
    if (!cJSON_IsObject(ptc)) return;
    int tool = pr->active_tool;
    if (tool < 0 || tool > 3) tool = 0;

    cJSON *colors = cJSON_GetObjectItem(ptc, "filament_color_rgba");
    if (cJSON_IsArray(colors)) {
        cJSON *v = cJSON_GetArrayItem(colors, tool);
        if (cJSON_IsString(v) && v->valuestring) safe_copy(pr->active_color_rgba, sizeof(pr->active_color_rgba), v->valuestring);
    }
    cJSON *types = cJSON_GetObjectItem(ptc, "filament_type");
    if (cJSON_IsArray(types)) {
        cJSON *v = cJSON_GetArrayItem(types, tool);
        if (cJSON_IsString(v) && v->valuestring) safe_copy(pr->active_material, sizeof(pr->active_material), v->valuestring);
    }
}

static void apply_chamber_ema(shu1_printer_state_t *pr, float raw) {
    if (!isfinite(raw)) return;
    if (!isfinite(pr->u1_chamber_temp)) pr->u1_chamber_temp = raw;
    else pr->u1_chamber_temp = SHU1_CHAMBER_TEMP_EMA_ALPHA * raw + (1.0f - SHU1_CHAMBER_TEMP_EMA_ALPHA) * pr->u1_chamber_temp;
}

static void parse_object_update(cJSON *objects) {
    if (!objects || !cJSON_IsObject(objects)) return;
    shu1_printer_state_t pr = shu1_state_get_printer();
    pr.moonraker_connected = true;
    pr.last_ws_message_ms = now_ms();

    cJSON *webhooks = cJSON_GetObjectItem(objects, "webhooks");
    if (cJSON_IsObject(webhooks)) {
        cJSON *state = cJSON_GetObjectItem(webhooks, "state");
        if (cJSON_IsString(state) && state->valuestring) safe_copy(pr.webhooks_state, sizeof(pr.webhooks_state), state->valuestring);
    }

    cJSON *ps = cJSON_GetObjectItem(objects, "print_stats");
    if (cJSON_IsObject(ps)) {
        cJSON *state = cJSON_GetObjectItem(ps, "state");
        if (cJSON_IsString(state) && state->valuestring) {
            safe_copy(pr.print_state, sizeof(pr.print_state), state->valuestring);
            safe_copy(pr.normalized_state, sizeof(pr.normalized_state), normalize_state(state->valuestring));
        }
        cJSON *filename = cJSON_GetObjectItem(ps, "filename");
        if (cJSON_IsString(filename) && filename->valuestring) safe_copy(pr.filename, sizeof(pr.filename), filename->valuestring);
        cJSON *pd = cJSON_GetObjectItem(ps, "print_duration");
        cJSON *td = cJSON_GetObjectItem(ps, "total_duration");
        if (cJSON_IsNumber(pd)) pr.print_duration_sec = (float)pd->valuedouble;
        if (cJSON_IsNumber(td)) pr.total_duration_sec = (float)td->valuedouble;
    }

    cJSON *ds = cJSON_GetObjectItem(objects, "display_status");
    if (cJSON_IsObject(ds)) {
        cJSON *progress = cJSON_GetObjectItem(ds, "progress");
        if (cJSON_IsNumber(progress)) pr.print_progress = (float)progress->valuedouble;
    }
    cJSON *vsd = cJSON_GetObjectItem(objects, "virtual_sdcard");
    if (cJSON_IsObject(vsd)) {
        cJSON *progress = cJSON_GetObjectItem(vsd, "progress");
        if (cJSON_IsNumber(progress) && pr.print_progress <= 0.0f) pr.print_progress = (float)progress->valuedouble;
    }
    if (pr.print_progress < 0.0f) pr.print_progress = 0.0f;
    if (pr.print_progress > 1.0f) pr.print_progress = 1.0f;

    update_active_tool_from_status(&pr, objects);
    update_tool_temp_from_status(&pr, objects);
    update_print_task_config(&pr, objects);

    cJSON *bed = cJSON_GetObjectItem(objects, "heater_bed");
    if (cJSON_IsObject(bed)) {
        cJSON *temperature = cJSON_GetObjectItem(bed, "temperature");
        cJSON *target = cJSON_GetObjectItem(bed, "target");
        if (cJSON_IsNumber(temperature)) pr.bed_temp = (float)temperature->valuedouble;
        if (cJSON_IsNumber(target)) pr.bed_target = (float)target->valuedouble;
    }

    if (g_chamber_object[0]) {
        cJSON *ch = cJSON_GetObjectItem(objects, g_chamber_object);
        if (cJSON_IsObject(ch)) {
            cJSON *temperature = cJSON_GetObjectItem(ch, "temperature");
            if (cJSON_IsNumber(temperature)) {
                apply_chamber_ema(&pr, (float)temperature->valuedouble);
                pr.chamber_sensor_online = true;
                safe_copy(pr.u1_chamber_object, sizeof(pr.u1_chamber_object), g_chamber_object);
            }
        }
    }

    if (g_cavity_fan_object[0]) {
        cJSON *fan = cJSON_GetObjectItem(objects, g_cavity_fan_object);
        if (cJSON_IsObject(fan)) {
            cJSON *speed = cJSON_GetObjectItem(fan, "speed");
            if (cJSON_IsNumber(speed)) {
                pr.cavity_fan_speed = (float)speed->valuedouble;
                if (pr.cavity_fan_speed < 0.0f) pr.cavity_fan_speed = 0.0f;
                if (pr.cavity_fan_speed > 1.0f) pr.cavity_fan_speed = 1.0f;
                pr.cavity_fan_online = true;
                safe_copy(pr.cavity_fan_object, sizeof(pr.cavity_fan_object), g_cavity_fan_object);
            }
        }
    }

    // Delta notifications and process statistics are not proof of fresh control data.
    cJSON *ps_state = cJSON_GetObjectItem(ps, "state");
    cJSON *bed_temp = cJSON_GetObjectItem(bed, "temperature");
    cJSON *bed_target = cJSON_GetObjectItem(bed, "target");
    cJSON *web_state = cJSON_GetObjectItem(webhooks, "state");
    if (cJSON_IsString(ps_state) && cJSON_IsString(web_state) &&
        cJSON_IsNumber(bed_temp) && isfinite(bed_temp->valuedouble) &&
        cJSON_IsNumber(bed_target) && isfinite(bed_target->valuedouble))
        pr.last_update_ms = now_ms();
    shu1_state_update_printer(&pr);
}

static void parse_moonraker_message(const char *data, int len) {
    if (!data || len <= 0) return;
    char *copy = calloc(1, len + 1);
    if (!copy) return;
    memcpy(copy, data, len);

    cJSON *root = shu1_json_parse(copy);
    free(copy);
    if (!root) return;

    cJSON *result = cJSON_GetObjectItem(root, "result");
    portENTER_CRITICAL(&g_symbiont_mux);
    shu1_symbiont_response(&g_symbiont,root,now_ms());
    portEXIT_CRITICAL(&g_symbiont_mux);
    int msg_id = cJSON_GetObjectItem(root, "id") ? cJSON_GetObjectItem(root, "id")->valueint : 0;

    if (cJSON_IsObject(result)) {
        if (msg_id == 100) {
            const char *klippy = cJSON_GetStringValue(cJSON_GetObjectItem(result, "klippy_state"));
            bool ready = klippy && strcmp(klippy, "ready") == 0;
            shu1_printer_state_t pr = shu1_state_get_printer();
            pr.klippy_ready = ready;
            pr.last_ws_message_ms = now_ms();
            shu1_state_update_printer(&pr);
            g_autodetect_pending = ready;
            if (!ready) ESP_LOGW(TAG, "Moonraker server.info: klippy_state=%s", klippy ? klippy : "-");
        }
        if (msg_id == 101 || msg_id == 102) {
            g_subscribe_pending = false;
            shu1_printer_state_t pr = shu1_state_get_printer();
            pr.subscribed = true;
            pr.autodetect_done = g_autodetect_done;
            pr.last_ws_message_ms = now_ms();
            shu1_state_update_printer(&pr);
            mark_online(true);
        }
        cJSON *status = cJSON_GetObjectItem(result, "status");
        if (cJSON_IsObject(status)) {
            if(msg_id==103 && shu1_printer_control_subset(status)) atomic_store(&g_setup_ws_valid,true);
            parse_object_update(status);
            mark_online(true);
            cJSON_Delete(root);
            return;
        }
    }

    const char *method = cJSON_GetStringValue(cJSON_GetObjectItem(root, "method"));
    if (method && strcmp(method, "notify_status_update") == 0) {
        cJSON *params = cJSON_GetObjectItem(root, "params");
        cJSON *status = cJSON_IsArray(params) ? cJSON_GetArrayItem(params, 0) : NULL;
        if (cJSON_IsObject(status)) {
            portENTER_CRITICAL(&g_symbiont_mux);
            shu1_symbiont_notify(&g_symbiont,status,now_ms());
            portEXIT_CRITICAL(&g_symbiont_mux);
            parse_object_update(status);
            mark_online(true);
        }
    } else if (method && strcmp(method, "notify_klippy_ready") == 0) {
        shu1_printer_state_t pr = shu1_state_get_printer();
        pr.klippy_ready = true;
        pr.last_ws_message_ms = now_ms();
        shu1_state_update_printer(&pr);
        g_autodetect_pending = true;
    } else if (method && (!strcmp(method, "notify_klippy_shutdown") || !strcmp(method, "notify_klippy_disconnected"))) {
        shu1_printer_state_t pr = shu1_state_get_printer();
        pr.klippy_ready = false;
        pr.subscribed = false;
        pr.autodetect_done = false;
        shu1_state_update_printer(&pr);
        g_subscribe_pending = false;
        g_autodetect_pending = false;
        g_autodetect_done = false;
    } else if (method && strcmp(method, "notify_proc_stat_update") == 0) {
        shu1_printer_state_t pr = shu1_state_get_printer();
        pr.last_ws_message_ms = now_ms();
        shu1_state_update_printer(&pr);
        mark_online(true);
    }

    cJSON_Delete(root);
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    (void)handler_args;
    (void)base;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED: {
        g_symbiont_aux=g_symbiont_top=false;
        memset(&g_rx, 0, sizeof(g_rx));
        ESP_LOGI(TAG, "Moonraker websocket connected");
        g_subscribe_pending = false;
        g_autodetect_pending = false;
        g_autodetect_done = false;
        g_has_tool_temp_subscription = false;
        g_has_chamber_subscription = false;
        mark_online(true);
        shu1_printer_state_t pr = shu1_state_get_printer();
        pr.last_ws_message_ms = now_ms();
        pr.last_update_ms = 0;
        pr.subscribed = false;
        pr.klippy_ready = false;
        shu1_state_update_printer(&pr);
        send_identify();
        vTaskDelay(pdMS_TO_TICKS(50));
        send_server_info();
        break;
    }
    case WEBSOCKET_EVENT_DISCONNECTED: {
        memset(&g_rx, 0, sizeof(g_rx));
        ESP_LOGW(TAG, "Moonraker websocket disconnected");
        g_subscribe_pending = false;
        g_autodetect_pending = false;
        mark_online(false);
        break;
    }
    case WEBSOCKET_EVENT_DATA:
        if (data) {
            size_t complete_length;
            if (shu1_ws_accumulate(&g_rx, data->op_code, data->fin, data->payload_len,
                                   data->payload_offset, data->data_ptr, data->data_len,
                                   &complete_length))
                parse_moonraker_message(g_rx.bytes, (int)complete_length);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "Moonraker websocket error");
        mark_online(false);
        break;
    default:
        break;
    }
}

static void clear_printer_context(void) {
    shu1_printer_state_t empty={0};
    shu1_state_update_printer(&empty);
    memset(&g_rx,0,sizeof(g_rx));
    g_subscribe_pending=g_autodetect_pending=g_autodetect_done=false;
    g_has_tool_temp_subscription=g_has_chamber_subscription=false;
    strcpy(g_chamber_object,"temperature_sensor cavity");g_cavity_fan_object[0]=0;
    g_last_autodetect_try_ms=0;g_last_connect_try_ms=0;
    atomic_store(&g_setup_ws_valid,false);
    g_symbiont_aux=g_symbiont_top=false;
    portENTER_CRITICAL(&g_symbiont_mux);
    g_symbiont.active=false;g_symbiont.pending=0;g_symbiont.phase=0;
    g_symbiont.status="disconnected";
    portEXIT_CRITICAL(&g_symbiont_mux);
}

static void symbiont_poll(void) {
    static shu1_state_t state; // Sole Moonraker worker; avoid a large task-stack snapshot.
    shu1_state_get(&state);
    const shu1_settings_t *st=&state.settings;
    const shu1_runtime_t *rt=&state.runtime;
    const int64_t now=now_ms();
    bool active=st->symbiont_mode_enabled && st->symbiont_ventilation_allowed &&
        st->symbiont_safe_control_enabled && st->symbiont_policy==SHU1_SYMBIONT_POLICY_CLIMATE_SAFE &&
        st->work_on && !st->user_paused && rt->heater_effective_target_c>0 &&
        rt->chamber_sensor_status==SHU1_SENSOR_OK && rt->ptc_sensor_status==SHU1_SENSOR_OK &&
        rt->last_sensor_ms>0 && now>=rt->last_sensor_ms && now-rt->last_sensor_ms<=1500 &&
        rt->heater_fault==SHU1_HEATER_OK && !shu1_safety_latch_is_set() &&
        !shu1_safety_latch_is_inhibited() && !shu1_control_maintenance_active();
    bool connected=g_client && esp_websocket_client_is_connected(g_client) &&
        state.printer.klippy_ready && state.printer.subscribed;
    char message[512];
    portENTER_CRITICAL(&g_symbiont_mux);
    bool send=shu1_symbiont_next(&g_symbiont,active,connected,g_symbiont_aux,
        g_symbiont_top,rt->symbiont_fan_percent,now,message,sizeof(message));
    const char *status=g_symbiont.status ? g_symbiont.status:"auto_or_idle";
    portEXIT_CRITICAL(&g_symbiont_mux);
    static bool reported_loss;
    bool unavailable=active && (!strcmp(status,"disconnected") || !strcmp(status,"unsupported_fan") ||
        !strcmp(status,"command_error") || !strcmp(status,"invalid_data") ||
        !strcmp(status,"timeout") || !strcmp(status,"top_cover_fault"));
    if(unavailable && !reported_loss) {
        shu1_event_log_add("warn","symbiont_control_lost","Printer ventilation control unavailable; Panda thermal protections remain active");
        reported_loss=true;
    } else if(reported_loss && active && !strcmp(status,"verified")) {
        shu1_event_log_add("info","symbiont_control_restored","Printer ventilation readback matches current request");
        reported_loss=false;
    } else if(!active) reported_loss=false;
    // Never hold the thermal policy guard during network I/O. One bounded
    // in-flight packet can finish after OFF/Auto; there is no queued replay.
    if(send) send_json(message);
}
static bool stop_client(void) {
    if(g_client) {
        (void)esp_websocket_client_stop(g_client);
        if(esp_websocket_client_destroy(g_client)!=ESP_OK) return false;
        g_client=NULL;
    }
    clear_printer_context();
    return true;
}
static bool create_client(void) {
    if(!g_devcfg.moonraker_host[0]) return false;
    char uri[192];
    snprintf(uri, sizeof(uri), "ws://%s:%d/websocket", g_devcfg.moonraker_host, g_devcfg.moonraker_port);
    char headers[160]="";
    if(g_devcfg.moonraker_api_key[0]) snprintf(headers,sizeof(headers),"X-Api-Key: %s\r\n",g_devcfg.moonraker_api_key);
    esp_websocket_client_config_t websocket_cfg = {
        .uri = uri,
        .headers = headers,
        .network_timeout_ms = 5000,
        .buffer_size = 4096,
        .reconnect_timeout_ms = SHU1_MOONRAKER_WS_RECONNECT_MS,
    };
    g_client = esp_websocket_client_init(&websocket_cfg);
    if (!g_client) {
        ESP_LOGE(TAG, "websocket init failed");
        return false;
    }
    if(esp_websocket_register_events(g_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL)!=ESP_OK) {
        esp_websocket_client_destroy(g_client);g_client=NULL;return false;
    }
    return true;
}
static const char *probe_printer(const shu1_device_config_t *candidate) {
    int code=0;
    char *body=http_get_for(candidate,"/server/info",1500,&code);
    if(!body) return code==401 || code==403 ? "auth_failed":"unreachable";
    cJSON *root=shu1_json_parse(body);free(body);
    const cJSON *result=cJSON_GetObjectItemCaseSensitive(root,"result");
    const char *state=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(result,"klippy_state"));
    bool ready=state && !strcmp(state,"ready");cJSON_Delete(root);
    if(!ready) return "printer_not_ready";
    body=http_get_for(candidate,"/printer/objects/query?print_stats&heater_bed&webhooks",1500,&code);
    if(!body) return code==401 || code==403 ? "auth_failed":"unreachable";
    root=shu1_json_parse(body);free(body);
    result=cJSON_GetObjectItemCaseSensitive(root,"result");
    bool valid=shu1_printer_control_subset(cJSON_GetObjectItemCaseSensitive(result,"status"));
    cJSON_Delete(root);
    return valid ? NULL:"missing_data";
}
static void apply_setup(setup_request_t *request) {
    const char *error=probe_printer(&request->cfg);
    shu1_device_config_t old=g_devcfg;
    bool switched=false;
    if(!error) {
        setup_phase("connecting");
        if(!stop_client()) {error="client_stop_failed";shu1_safety_latch_inhibit();}
        else {
            switched=true;g_devcfg=request->cfg;
            if(!create_client() || esp_websocket_client_start(g_client)!=ESP_OK) error="websocket_failed";
            else {
                const int64_t deadline=now_ms()+20000;
                while(now_ms()<deadline) {
                    if(esp_websocket_client_is_connected(g_client)) {
                        shu1_printer_state_t pr=shu1_state_get_printer();
                        if(pr.klippy_ready && !pr.subscribed && !g_subscribe_pending) send_subscription(false);
                        send_json(CONTROL_QUERY);
                        if(pr.klippy_ready && pr.subscribed && atomic_load(&g_setup_ws_valid) && pr.last_update_ms>0 && now_ms()-pr.last_update_ms<1500) break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(250));
                }
                shu1_printer_state_t pr=shu1_state_get_printer();
                if(!esp_websocket_client_is_connected(g_client) || !pr.klippy_ready || !pr.subscribed ||
                   !atomic_load(&g_setup_ws_valid) || pr.last_update_ms<=0 ||
                   now_ms()-pr.last_update_ms<0 || now_ms()-pr.last_update_ms>=1500) error="websocket_failed";
            }
        }
    }
    if(!error) {
        setup_phase("saving");
        if(shu1_settings_store_save_moonraker(&request->cfg)!=ESP_OK) {
            error="storage_failed";
            if(shu1_settings_store_save_moonraker(&old)!=ESP_OK) shu1_safety_latch_inhibit();
        }
    }
    if(error && switched) {
        if(stop_client()) {g_devcfg=old;(void)create_client();}
        else shu1_safety_latch_inhibit();
    }
    if(!error) {
        portENTER_CRITICAL(&g_setup_mux);g_key_set=g_devcfg.moonraker_api_key[0]!=0;portEXIT_CRITICAL(&g_setup_mux);
    }
    memset(&old,0,sizeof(old));memset(request,0,sizeof(*request));
    // No thermal loop or network operation ran while holding the policy guard.
    SHU1_CONTROL_GUARD(guard);
    shu1_control_release_any();
    setup_phase(error ? error:"succeeded");
    shu1_control_maintenance_end();
    shu1_event_log_add(error ? "warn":"info",error ? "printer_setup_failed":"printer_setup_saved",
        error ? "printer configuration test failed; previous configuration retained":"printer configuration tested and applied without reboot");
}
static void moonraker_task(void *arg) {
    (void)arg;
    shu1_device_config_defaults(&g_devcfg);
    shu1_settings_store_load_device_config(&g_devcfg);
    if (g_devcfg.moonraker_port <= 0 || g_devcfg.moonraker_port > 65535) g_devcfg.moonraker_port = 7125;
    portENTER_CRITICAL(&g_setup_mux);g_key_set=g_devcfg.moonraker_api_key[0]!=0;portEXIT_CRITICAL(&g_setup_mux);
    (void)create_client();

    int64_t last_control_query=0;
    while (true) {
        setup_request_t request;
        if(xQueueReceive(g_setup_queue,&request,0)==pdTRUE) {apply_setup(&request);continue;}
        symbiont_poll();
        if(!g_client) {(void)create_client();vTaskDelay(pdMS_TO_TICKS(500));continue;}
        if (!esp_websocket_client_is_connected(g_client)) {
            int64_t now = now_ms();
            if (now - g_last_connect_try_ms >= SHU1_MOONRAKER_WS_RECONNECT_MS) {
                g_last_connect_try_ms = now;
                ESP_LOGI(TAG, "connecting to configured Moonraker");
                esp_websocket_client_start(g_client);
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        shu1_printer_state_t pr = shu1_state_get_printer();
        int64_t now = now_ms();
        if (pr.last_ws_message_ms > 0 && now - pr.last_ws_message_ms > SHU1_MOONRAKER_WS_STALE_MS) {
            ESP_LOGW(TAG, "Moonraker websocket stale for %lld ms; reconnecting", (long long)(now - pr.last_ws_message_ms));
            esp_websocket_client_stop(g_client);
            mark_online(false);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (g_autodetect_pending && !g_subscribe_pending) {
            moonraker_autodetect_and_subscribe();
        } else if (!pr.subscribed && !g_subscribe_pending && !g_autodetect_pending) {
            send_subscription(false);
        }

        if(now-last_control_query<1000) {vTaskDelay(pdMS_TO_TICKS(100));continue;}
        last_control_query=now;
        // Query the complete control subset, because subscriptions send only changes.
        send_json("{\"jsonrpc\":\"2.0\",\"id\":103,\"method\":\"printer.objects.query\","
                  "\"params\":{\"objects\":{\"print_stats\":[\"state\"],"
                  "\"heater_bed\":[\"temperature\",\"target\"],\"webhooks\":[\"state\"]}}}");
        // Keep server.info flowing; it also gives us a lightweight liveness check.
        char ping[96];
        snprintf(ping, sizeof(ping), "{\"id\":%d,\"jsonrpc\":\"2.0\",\"method\":\"server.info\"}", g_rpc_id++);
        send_json(ping);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t shu1_moonraker_start(void) {
    g_setup_queue=xQueueCreate(1,sizeof(setup_request_t));
    if(!g_setup_queue) return ESP_ERR_NO_MEM;
    BaseType_t ok = xTaskCreate(moonraker_task, "moonraker_ws", 8192, NULL, 5, NULL);
    if(ok!=pdPASS) {vQueueDelete(g_setup_queue);g_setup_queue=NULL;return ESP_ERR_NO_MEM;}
    return ESP_OK;
}
