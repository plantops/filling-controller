#include "sp01/web_hmi.hpp"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace sp01 {
namespace {

constexpr char kTag[] = "web_hmi";
WebHmiConfig g_config{};
HmiSnapshotFn g_snapshot_fn = nullptr;
HmiCalZeroFn g_zero_fn = nullptr;
HmiCalSpanFn g_span_fn = nullptr;
httpd_handle_t g_server = nullptr;

constexpr char kIndexHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SP01 Filling Controller</title>
<style>
body{font-family:system-ui,sans-serif;max-width:900px;margin:20px auto;padding:0 14px;background:#111;color:#eee}
h1{font-size:22px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}
.card{border:1px solid #555;border-radius:8px;padding:12px}table{width:100%;border-collapse:collapse}td{padding:3px 5px;border-bottom:1px solid #333}
.on{font-weight:700}.bad{font-weight:700}button,input{font:inherit;padding:7px;margin:3px}small{opacity:.75}
</style></head><body>
<h1>SP01 Filling Controller</h1><div id="offline">Connecting / Đang kết nối…</div>
<div class="grid">
<div class="card"><b>Status / Trạng thái</b><table><tr><td>State</td><td id="state">-</td></tr><tr><td>Fault</td><td id="fault">-</td></tr><tr><td>Cycle</td><td id="cycle">-</td></tr><tr><td>Weight / Khối lượng</td><td id="weight">-</td></tr><tr><td>Stable</td><td id="stable">-</td></tr></table></div>
<div class="card"><b>I/O</b><table id="io"></table></div>
<div class="card"><b>Calibration / Hiệu chuẩn</b><p>Service ready: <b id="svc">NO</b></p><button onclick="zero()">SET ZERO</button><br><button onclick="check(20)">CHECK 20 kg</button><span id="c20"></span><br><button onclick="span50()">SET SPAN 50 kg</button><br><button onclick="check(50)">VERIFY 50 kg</button><span id="c50"></span><p><small>Calibration writes require DI8 service enable, machine permissive OFF, stable weight, and service token.</small></p></div>
<div class="card"><b>Diagnostics / Chẩn đoán</b><pre id="diag">-</pre></div>
</div>
<script>
let last=null,token=sessionStorage.getItem('sp01token')||'';
function bits(v){let s='';for(let i=0;i<8;i++)s+=`<tr><td>DI${i+1}</td><td class="${v.di&(1<<i)?'on':''}">${v.di&(1<<i)?'ON':'OFF'}</td><td>DO${i+1}</td><td class="${v.do&(1<<i)?'on':''}">${v.do&(1<<i)?'ON':'OFF'}</td></tr>`;return s}
async function poll(){try{const r=await fetch('/api/state',{cache:'no-store'});if(!r.ok)throw 0;last=await r.json();offline.textContent='LIVE';state.textContent=last.state;fault.textContent=last.fault;cycle.textContent=last.cycle;weight.textContent=last.weight.toFixed(3)+' kg';stable.textContent=last.stable?'YES':'NO';svc.textContent=last.service_ready?'YES':'NO';io.innerHTML=bits(last);diag.textContent=`quality=${last.quality}\nTLB polls=${last.tlb_polls}\nTLB errors=${last.tlb_errors}\nTLB last=${last.tlb_last_error}`;}catch(e){offline.textContent='OFFLINE / MẤT KẾT NỐI'}setTimeout(poll,250)}
function auth(){if(!token){token=prompt('Service token')||'';sessionStorage.setItem('sp01token',token)}return {'X-Service-Token':token}}
async function post(path,body=''){const r=await fetch(path,{method:'POST',headers:{...auth(),'Content-Type':'text/plain'},body});const t=await r.text();alert(r.status+' '+t);if(r.status===401||r.status===403){token='';sessionStorage.removeItem('sp01token')}}
function zero(){post('/api/cal/zero')}
function span50(){post('/api/cal/span','50.000')}
function check(k){if(!last)return;document.getElementById(k===20?'c20':'c50').textContent='  reading '+last.weight.toFixed(3)+' kg, error '+(last.weight-k).toFixed(3)+' kg'}
poll();
</script></body></html>)HTML";

std::uint8_t pack_inputs(const InputImage& in) noexcept {
    std::uint8_t v = 0;
    for (std::size_t i = 0; i < in.di.size(); ++i) if (in.di[i]) v |= 1U << i;
    return v;
}

std::uint8_t pack_outputs(const OutputImage& out) noexcept {
    std::uint8_t v = 0;
    for (std::size_t i = 0; i < out.channels.size(); ++i) if (out.channels[i]) v |= 1U << i;
    return v;
}

bool token_ok(httpd_req_t* req) noexcept {
    if (!g_config.service_token || g_config.service_token[0] == '\0') return false;
    char value[96]{};
    if (httpd_req_get_hdr_value_str(req, "X-Service-Token", value, sizeof(value)) != ESP_OK) return false;
    return std::strcmp(value, g_config.service_token) == 0;
}

esp_err_t send_text(httpd_req_t* req, const char* text, const char* type = "text/plain") noexcept {
    httpd_resp_set_type(req, type);
    return httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
}

esp_err_t root_handler(httpd_req_t* req) noexcept {
    return send_text(req, kIndexHtml, "text/html");
}

esp_err_t state_handler(httpd_req_t* req) noexcept {
    if (!g_snapshot_fn) return httpd_resp_send_err(req, HTTPD_503_SERVICE_UNAVAILABLE, "status unavailable");
    HmiSnapshot s{};
    if (!g_snapshot_fn(s)) return httpd_resp_send_err(req, HTTPD_503_SERVICE_UNAVAILABLE, "status unavailable");
    char json[512]{};
    std::snprintf(json, sizeof(json),
                  "{\"state\":\"%s\",\"fault\":\"%s\",\"cycle\":%u,\"weight\":%.3f,"
                  "\"stable\":%s,\"quality\":%u,\"di\":%u,\"do\":%u,\"service_ready\":%s,"
                  "\"tlb_polls\":%u,\"tlb_errors\":%u,\"tlb_last_error\":%d}",
                  state_name(s.controller.state), fault_name(s.controller.fault), s.controller.cycle_id,
                  static_cast<double>(s.weight.net_kg), s.weight.stable ? "true" : "false",
                  static_cast<unsigned>(s.weight.quality), pack_inputs(s.inputs), pack_outputs(s.controller.outputs),
                  s.service_ready ? "true" : "false", s.tlb.polls_ok, s.tlb.comm_errors,
                  static_cast<int>(s.tlb.last_error));
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return send_text(req, json, "application/json");
}

bool service_request_allowed(httpd_req_t* req) noexcept {
    if (!token_ok(req)) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "bad service token");
        return false;
    }
    HmiSnapshot s{};
    if (!g_snapshot_fn || !g_snapshot_fn(s) || !s.service_ready) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "service interlock not ready");
        return false;
    }
    return true;
}

esp_err_t zero_handler(httpd_req_t* req) noexcept {
    if (!service_request_allowed(req)) return ESP_OK;
    const esp_err_t err = g_zero_fn ? g_zero_fn() : ESP_ERR_NOT_SUPPORTED;
    if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
    return send_text(req, "OK zero captured");
}

esp_err_t span_handler(httpd_req_t* req) noexcept {
    if (!service_request_allowed(req)) return ESP_OK;
    char body[32]{};
    const int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing reference kg");
    body[n] = '\0';
    char* end = nullptr;
    const float kg = std::strtof(body, &end);
    if (end == body || kg <= 0.0F) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid reference kg");
    const esp_err_t err = g_span_fn ? g_span_fn(kg) : ESP_ERR_NOT_SUPPORTED;
    if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
    return send_text(req, "OK span captured");
}

void wifi_event(void*, esp_event_base_t base, std::int32_t id, void*) noexcept {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        (void)esp_wifi_connect();
    }
}

esp_err_t start_http() noexcept {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.task_priority = 4;
    cfg.stack_size = 6144;
    cfg.max_open_sockets = 4;
    cfg.lru_purge_enable = true;
    esp_err_t err = httpd_start(&g_server, &cfg);
    if (err != ESP_OK) return err;

    httpd_uri_t root{}; root.uri = "/"; root.method = HTTP_GET; root.handler = root_handler;
    httpd_uri_t state{}; state.uri = "/api/state"; state.method = HTTP_GET; state.handler = state_handler;
    httpd_uri_t zero{}; zero.uri = "/api/cal/zero"; zero.method = HTTP_POST; zero.handler = zero_handler;
    httpd_uri_t span{}; span.uri = "/api/cal/span"; span.method = HTTP_POST; span.handler = span_handler;
    httpd_register_uri_handler(g_server, &root);
    httpd_register_uri_handler(g_server, &state);
    httpd_register_uri_handler(g_server, &zero);
    httpd_register_uri_handler(g_server, &span);
    return ESP_OK;
}

}  // namespace

esp_err_t web_hmi_start(const WebHmiConfig& config,
                        HmiSnapshotFn snapshot_fn,
                        HmiCalZeroFn zero_fn,
                        HmiCalSpanFn span_fn) noexcept {
    g_config = config;
    g_snapshot_fn = snapshot_fn;
    g_zero_fn = zero_fn;
    g_span_fn = span_fn;

    if (!config.ssid || config.ssid[0] == '\0') {
        ESP_LOGW(kTag, "Wi-Fi SSID empty; HMI disabled");
        return ESP_OK;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    ESP_ERROR_CHECK(esp_netif_init());
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    if ((err = esp_wifi_init(&wifi_cfg)) != ESP_OK) return err;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event, nullptr));
    wifi_config_t sta{};
    std::snprintf(reinterpret_cast<char*>(sta.sta.ssid), sizeof(sta.sta.ssid), "%s", config.ssid);
    std::snprintf(reinterpret_cast<char*>(sta.sta.password), sizeof(sta.sta.password), "%s", config.password ? config.password : "");
    sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) return err;
    if ((err = esp_wifi_set_config(WIFI_IF_STA, &sta)) != ESP_OK) return err;
    if ((err = esp_wifi_start()) != ESP_OK) return err;
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_connect();

    ESP_RETURN_ON_ERROR(start_http(), kTag, "HTTP start");
    ESP_LOGI(kTag, "HMI started; Wi-Fi STA connecting to %s", config.ssid);
    return ESP_OK;
}

}  // namespace sp01
