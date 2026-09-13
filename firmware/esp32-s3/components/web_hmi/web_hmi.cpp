#include "sp01/web_hmi.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern const char op_html_start[] asm("_binary_op_html_start");
extern const char op_html_end[] asm("_binary_op_html_end");

namespace sp01 {
namespace {

constexpr char kTag[] = "hmi";

// Wrong-PIN handling. Four digits is 10,000 combinations, which a script walks
// in seconds without this. The lockout is the only thing that makes a short PIN
// worth anything.
constexpr int kMaxPinAttempts = 5;
constexpr std::uint64_t kLockoutUs = 30ULL * 1000000ULL;

httpd_handle_t g_server = nullptr;
HmiIdentity g_identity{};
HmiPins g_pins{};
HmiCallbacks g_cb{};

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
HmiPublish g_state{};

int g_pin_failures = 0;
std::uint64_t g_locked_until_us = 0;

HmiPublish snapshot_copy() {
    portENTER_CRITICAL(&g_mux);
    HmiPublish copy = g_state;
    portEXIT_CRITICAL(&g_mux);
    return copy;
}

bool locked_out(std::uint64_t now_us) {
    return now_us < g_locked_until_us;
}

// Body is form-encoded: "kg=50.3&pin=1111".
bool read_body(httpd_req_t* req, char* buf, std::size_t cap) {
    const std::size_t len = req->content_len;
    if (len == 0 || len >= cap) return false;
    std::size_t got = 0;
    while (got < len) {
        const int r = httpd_req_recv(req, buf + got, len - got);
        if (r <= 0) return false;
        got += static_cast<std::size_t>(r);
    }
    buf[got] = '\0';
    return true;
}

bool field(const char* body, const char* key, char* out, std::size_t cap) {
    char pattern[24];
    std::snprintf(pattern, sizeof(pattern), "%s=", key);
    const char* p = std::strstr(body, pattern);
    if (p == nullptr) return false;
    p += std::strlen(pattern);
    std::size_t i = 0;
    while (*p != '\0' && *p != '&' && i + 1 < cap) out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

// ------------------------------------------------------------------ handlers

esp_err_t index_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, op_html_start,
                           static_cast<ssize_t>(op_html_end - op_html_start - 1));
}

std::size_t blocks_json(const Explain& e, char* out, std::size_t cap) {
    std::size_t n = 0;
    n += static_cast<std::size_t>(std::snprintf(out + n, cap - n, "["));
    bool first = true;
    for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(Block::Count); ++i) {
        const auto b = static_cast<Block>(i);
        if (!e.blocked_by(b)) continue;
        n += static_cast<std::size_t>(std::snprintf(
            out + n, cap - n, "%s{\"id\":\"%s\",\"text\":\"%s\",\"en\":\"%s\"}",
            first ? "" : ",", block_name(b), block_text_vi(b), block_text_en(b)));
        first = false;
        if (n + 160 >= cap) break;
    }
    n += static_cast<std::size_t>(std::snprintf(out + n, cap - n, "]"));
    return n;
}

esp_err_t state_handler(httpd_req_t* req) {
    const HmiPublish s = snapshot_copy();

    char blocks[768];
    blocks_json(s.explain, blocks, sizeof(blocks));

    char date[16] = "";
    char clock[12] = "";
    char span[16] = "";
    if (s.time_synced) {
        std::snprintf(date, sizeof(date), "%02u/%02u/%04u", s.civil.day,
                      s.civil.month, s.civil.year);
        std::snprintf(clock, sizeof(clock), "%02u:%02u:%02u", s.civil.hour,
                      s.civil.minute, s.civil.second);
        const unsigned start = (s.shift_no > 0 ? s.shift_no - 1 : 0) * kShiftHours;
        std::snprintf(span, sizeof(span), "%02u:00\u2013%02u:00", start,
                      (start + kShiftHours) % 24);
    }

    // Build the optional fragments first: a static buffer inside the format
    // call would be shared between concurrent requests.
    char pending[40] = "";
    if (s.target_pending_kg > 0.0F) {
        std::snprintf(pending, sizeof(pending), "\"target_pending_kg\":%.1f,",
                      static_cast<double>(s.target_pending_kg));
    }
    char last_bag[16] = "null";
    if (s.has_last_bag) {
        std::snprintf(last_bag, sizeof(last_bag), "%.2f",
                      static_cast<double>(s.last_bag_kg));
    }

    char body[1600];
    const int n = std::snprintf(
        body, sizeof(body),
        "{\"machine_id\":\"%s\",\"spout_id\":\"%s\",\"fw\":\"%s\","
        "\"state\":\"%s\",\"fault\":\"%s\",\"mode\":\"%s\","
        "\"weight_kg\":%.2f,\"target_kg\":%.1f,%s"
        "\"last_bag_kg\":%s,\"last_bag_time\":\"%s\","
        "\"shift_bags\":%lu,\"day_bags\":%lu,\"shift_rejects\":%lu,"
        "\"shift_kg\":%.1f,\"day_kg\":%.1f,"
        "\"shift_no\":%u,\"shift_span\":\"%s\",\"date\":\"%s\","
        "\"clock\":\"%s\",\"time_synced\":%s,\"unattributed_bags\":%lu,"
        "\"ready\":%s,\"angle_deg\":%.1f,\"position_valid\":%s,"
        "\"blocks\":%s}",
        g_identity.machine, g_identity.spout, g_identity.firmware,
        state_name(s.snapshot.state), fault_name(s.snapshot.fault),
        mode_name(s.snapshot.mode), static_cast<double>(s.weight_kg),
        static_cast<double>(s.target_kg), pending, last_bag, s.last_bag_time,
        static_cast<unsigned long>(s.shift.bags),
        static_cast<unsigned long>(s.day.bags),
        static_cast<unsigned long>(s.shift.rejects),
        static_cast<double>(s.shift.kg), static_cast<double>(s.day.kg),
        s.shift_no, span, date, clock, s.time_synced ? "true" : "false",
        static_cast<unsigned long>(s.unattributed.bags),
        s.explain.ready ? "true" : "false",
        static_cast<double>(s.snapshot.angle_deg),
        s.snapshot.position_valid ? "true" : "false", blocks);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, n);
}

esp_err_t target_handler(httpd_req_t* req) {
    const std::uint64_t now = static_cast<std::uint64_t>(esp_timer_get_time());
    if (locked_out(now)) {
        httpd_resp_set_status(req, "429 Too Many Requests");
        return httpd_resp_sendstr(req, "locked");
    }

    char body[96];
    if (!read_body(req, body, sizeof(body))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "kg and pin required");
        return ESP_FAIL;
    }

    char kg_text[16];
    char pin[12];
    if (!field(body, "kg", kg_text, sizeof(kg_text)) ||
        !field(body, "pin", pin, sizeof(pin))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "kg and pin required");
        return ESP_FAIL;
    }

    if (std::strcmp(pin, g_pins.operator_pin) != 0) {
        ++g_pin_failures;
        if (g_pin_failures >= kMaxPinAttempts) {
            g_locked_until_us = now + kLockoutUs;
            g_pin_failures = 0;
            ESP_LOGW(kTag, "PIN entry locked for 30 s after %d failures",
                     kMaxPinAttempts);
        }
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_sendstr(req, "bad pin");
    }
    g_pin_failures = 0;

    const float kg = std::strtof(kg_text, nullptr);
    if (kg < 5.0F || kg > 100.0F) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "out of range");
        return ESP_FAIL;
    }

    if (g_cb.request_target == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no handler");
        return ESP_FAIL;
    }

    const HmiResult r = g_cb.request_target(kg);
    if (r != HmiResult::Ok) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "rejected");
    }
    ESP_LOGI(kTag, "target change accepted: %.1f kg", static_cast<double>(kg));
    return httpd_resp_sendstr(req, "ok");
}

esp_err_t time_handler(httpd_req_t* req) {
    char body[96];
    if (!read_body(req, body, sizeof(body))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unix_ms required");
        return ESP_FAIL;
    }
    char ms_text[24];
    char tz_text[12];
    if (!field(body, "unix_ms", ms_text, sizeof(ms_text))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unix_ms required");
        return ESP_FAIL;
    }
    const std::int16_t tz =
        field(body, "tz", tz_text, sizeof(tz_text))
            ? static_cast<std::int16_t>(std::atoi(tz_text))
            : 0;
    const std::uint64_t unix_ms = std::strtoull(ms_text, nullptr, 10);

    if (g_cb.set_time != nullptr) g_cb.set_time(unix_ms, tz);
    return httpd_resp_sendstr(req, "ok");
}

}  // namespace

void hmi_publish(const HmiPublish& state) {
    portENTER_CRITICAL(&g_mux);
    g_state = state;
    portEXIT_CRITICAL(&g_mux);
}

esp_err_t hmi_start(const HmiIdentity& identity, const HmiPins& pins,
                    const HmiCallbacks& callbacks) {
    g_identity = identity;
    g_pins = pins;
    g_cb = callbacks;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 8;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 8192;

    esp_err_t err = httpd_start(&g_server, &cfg);
    if (err != ESP_OK) return err;

    const httpd_uri_t routes[] = {
        {"/", HTTP_GET, index_handler, nullptr},
        {"/api/state", HTTP_GET, state_handler, nullptr},
        {"/api/op/target", HTTP_POST, target_handler, nullptr},
        {"/api/time", HTTP_POST, time_handler, nullptr},
    };
    for (const auto& r : routes) {
        err = httpd_register_uri_handler(g_server, &r);
        if (err != ESP_OK) return err;
    }
    ESP_LOGI(kTag, "operator HMI ready");
    return ESP_OK;
}

}  // namespace sp01
