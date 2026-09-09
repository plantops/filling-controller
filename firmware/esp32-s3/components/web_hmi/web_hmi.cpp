#include "sp01/web_hmi.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_eth.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"
#include "nvs_flash.h"

#include <cinttypes>
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
HmiBenchDoPulseFn g_bench_do_pulse_fn = nullptr;
HmiBenchDoOffFn g_bench_do_off_fn = nullptr;
httpd_handle_t g_server = nullptr;
esp_eth_handle_t g_eth = nullptr;
esp_netif_t* g_eth_netif = nullptr;

constexpr spi_host_device_t kEthSpiHost = SPI2_HOST;
constexpr gpio_num_t kEthSck = GPIO_NUM_15;
constexpr gpio_num_t kEthMosi = GPIO_NUM_13;
constexpr gpio_num_t kEthMiso = GPIO_NUM_14;
constexpr gpio_num_t kEthCs = GPIO_NUM_16;
constexpr gpio_num_t kEthIrq = GPIO_NUM_12;
constexpr int kEthReset = 39;
constexpr int kEthPhyAddress = 1;
constexpr int kEthSpiHz = 20 * 1000 * 1000;
constexpr std::uint32_t kBenchPulseMs = 500;

constexpr char kIndexHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SP01 Filling Controller</title>
<style>
body{font-family:system-ui,sans-serif;max-width:980px;margin:20px auto;padding:0 14px;background:#111;color:#eee}
h1{font-size:22px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:12px}
.card{border:1px solid #555;border-radius:8px;padding:12px}table{width:100%;border-collapse:collapse}td{padding:3px 5px;border-bottom:1px solid #333}
.on{font-weight:700}.bad{font-weight:700}button,input{font:inherit;padding:7px;margin:3px}small{opacity:.75}.warn{border-color:#a66}
</style></head><body>
<h1>SP01 Filling Controller</h1><div id="offline">Connecting / Đang kết nối…</div>
<div class="grid">
<div class="card"><b>Status / Trạng thái</b><table><tr><td>Mode</td><td id="mode">-</td></tr><tr><td>State</td><td id="state">-</td></tr><tr><td>Fault</td><td id="fault">-</td></tr><tr><td>Cycle</td><td id="cycle">-</td></tr><tr><td>Weight / Khối lượng</td><td id="weight">-</td></tr><tr><td>Stable</td><td id="stable">-</td></tr></table></div>
<div class="card"><b>I/O</b><table id="io"></table></div>
<div class="card warn" id="bench" style="display:none"><b>G2 BENCH DO TEST</b><p><small>MACHINE / SOLENOID / CONTACTOR MUST BE DISCONNECTED. One output only, automatic OFF after 500 ms.</small></p><div id="dobtn"></div><button onclick="benchOff()">ALL OFF</button></div>
<div class="card"><b>Calibration / Hiệu chuẩn</b><p>Service ready: <b id="svc">NO</b></p><button onclick="zero()">SET ZERO</button><br><button onclick="check(20)">CHECK 20 kg</button><span id="c20"></span><br><button onclick="span50()">SET SPAN 50 kg</button><br><button onclick="check(50)">VERIFY 50 kg</button><span id="c50"></span><p><small>Calibration writes require machine stopped, fill switch OFF, all outputs safe, fresh stable weight, calibration writes enabled, and service token.</small></p></div>
<div class="card"><b>Diagnostics / Chẩn đoán</b><pre id="diag">-</pre></div>
</div>
<script>
let last=null,token=sessionStorage.getItem('sp01token')||'';
function bits(v){let s='';for(let i=0;i<8;i++)s+=`<tr><td>DI${i+1}</td><td class="${v.di&(1<<i)?'on':''}">${v.di&(1<<i)?'ON':'OFF'}</td><td>DO${i+1}</td><td class="${v.do&(1<<i)?'on':''}">${v.do&(1<<i)?'ON':'OFF'}</td></tr>`;return s}
function makeDoButtons(){let s='';for(let i=1;i<=8;i++)s+=`<button onclick="benchDo(${i})">PULSE DO${i}</button>`;dobtn.innerHTML=s}
async function poll(){try{const r=await fetch('/api/state',{cache:'no-store'});if(!r.ok)throw 0;last=await r.json();offline.textContent='LIVE';mode.textContent=last.mode;state.textContent=last.state;fault.textContent=last.fault;cycle.textContent=last.cycle;weight.textContent=last.weight.toFixed(3)+' kg';stable.textContent=last.stable?'YES':'NO';svc.textContent=last.service_ready?'YES':'NO';io.innerHTML=bits(last);bench.style.display=last.bench_do_available?'block':'none';diag.textContent=`quality=${last.quality}\nTLB polls=${last.tlb_polls}\nTLB errors=${last.tlb_errors}\nTLB last=${last.tlb_last_error}`;}catch(e){offline.textContent='OFFLINE / MẤT KẾT NỐI'}setTimeout(poll,250)}
function auth(){if(!token){token=prompt('Service token')||'';sessionStorage.setItem('sp01token',token)}return {'X-Service-Token':token}}
async function post(path,body=''){const r=await fetch(path,{method:'POST',headers:{...auth(),'Content-Type':'text/plain'},body});const t=await r.text();alert(r.status+' '+t);if(r.status===401||r.status===403){token='';sessionStorage.removeItem('sp01token')}}
async function benchPost(body){const r=await fetch('/api/bench/do',{method:'POST',headers:{'Content-Type':'text/plain'},body});const t=await r.text();if(!r.ok)alert(r.status+' '+t)}
function benchDo(n){if(confirm(`Pulse DO${n} for 500 ms? Machine wiring MUST be disconnected.`))benchPost(String(n))}
function benchOff(){benchPost('0')}
function zero(){post('/api/cal/zero')}
function span50(){post('/api/cal/span','50.000')}
function check(k){if(!last)return;document.getElementById(k===20?'c20':'c50').textContent='  reading '+last.weight.toFixed(3)+' kg, error '+(last.weight-k).toFixed(3)+' kg'}
makeDoButtons();poll();
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
    if (!g_snapshot_fn) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "status unavailable");
    HmiSnapshot s{};
    if (!g_snapshot_fn(s)) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "status unavailable");
    char json[704]{};
    std::snprintf(json, sizeof(json),
                  "{\"mode\":\"%s\",\"state\":\"%s\",\"fault\":\"%s\",\"cycle\":%" PRIu32 ",\"weight\":%.3f,"
                  "\"stable\":%s,\"quality\":%u,\"di\":%u,\"do\":%u,\"service_ready\":%s,\"bench_do_available\":%s,"
                  "\"tlb_polls\":%" PRIu32 ",\"tlb_errors\":%" PRIu32 ",\"tlb_last_error\":%d}",
                  mode_name(s.controller.mode), state_name(s.controller.state), fault_name(s.controller.fault),
                  s.controller.cycle_id, static_cast<double>(s.weight.net_kg),
                  s.weight.stable ? "true" : "false", static_cast<unsigned>(s.weight.quality),
                  static_cast<unsigned>(pack_inputs(s.inputs)), static_cast<unsigned>(pack_outputs(s.controller.outputs)),
                  s.service_ready ? "true" : "false", g_config.bench_do_enabled ? "true" : "false",
                  s.tlb.polls_ok, s.tlb.comm_errors, static_cast<int>(s.tlb.last_error));
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

esp_err_t bench_do_handler(httpd_req_t* req) noexcept {
    if (!g_config.bench_do_enabled || !g_bench_do_pulse_fn || !g_bench_do_off_fn) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "bench DO test disabled");
    }
    char body[16]{};
    const int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "channel required");
    body[n] = '\0';
    char* end = nullptr;
    const long channel = std::strtol(body, &end, 10);
    if (end == body || *end != '\0' || channel < 0 || channel > 8) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "channel must be 0..8");
    }
    if (channel == 0) {
        const esp_err_t err = g_bench_do_off_fn();
        if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
        return send_text(req, "OK all DO off");
    }
    const esp_err_t err = g_bench_do_pulse_fn(static_cast<std::uint8_t>(channel), kBenchPulseMs);
    if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
    char reply[48]{};
    std::snprintf(reply, sizeof(reply), "OK DO%ld pulse %" PRIu32 " ms", channel, kBenchPulseMs);
    return send_text(req, reply);
}

void wifi_event(void*, esp_event_base_t base, std::int32_t id, void*) noexcept {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        (void)esp_wifi_connect();
    }
}

void eth_event(void*, esp_event_base_t, std::int32_t id, void*) noexcept {
    if (id == ETHERNET_EVENT_CONNECTED) ESP_LOGI(kTag, "Ethernet link up");
    else if (id == ETHERNET_EVENT_DISCONNECTED) ESP_LOGW(kTag, "Ethernet link down");
}

void eth_got_ip(void*, esp_event_base_t, std::int32_t, void* event_data) noexcept {
    const auto* ev = static_cast<ip_event_got_ip_t*>(event_data);
    ESP_LOGI(kTag, "Ethernet DHCP IP: " IPSTR, IP2STR(&ev->ip_info.ip));
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
    httpd_uri_t bench{}; bench.uri = "/api/bench/do"; bench.method = HTTP_POST; bench.handler = bench_do_handler;
    httpd_register_uri_handler(g_server, &root);
    httpd_register_uri_handler(g_server, &state);
    httpd_register_uri_handler(g_server, &zero);
    httpd_register_uri_handler(g_server, &span);
    httpd_register_uri_handler(g_server, &bench);
    return ESP_OK;
}

esp_err_t start_ethernet() noexcept {
    spi_bus_config_t bus_cfg{};
    bus_cfg.miso_io_num = kEthMiso;
    bus_cfg.mosi_io_num = kEthMosi;
    bus_cfg.sclk_io_num = kEthSck;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(kEthSpiHost, &bus_cfg, SPI_DMA_CH_AUTO), kTag, "ETH SPI bus init");

    spi_device_interface_config_t spi_dev{};
    spi_dev.mode = 0;
    spi_dev.clock_speed_hz = kEthSpiHz;
    spi_dev.queue_size = 16;
    spi_dev.spics_io_num = kEthCs;

    eth_w5500_config_t w5500 = ETH_W5500_DEFAULT_CONFIG(kEthSpiHost, &spi_dev);
    w5500.int_gpio_num = kEthIrq;

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    mac_cfg.rx_task_stack_size = 4096;
    esp_eth_mac_t* mac = esp_eth_mac_new_w5500(&w5500, &mac_cfg);
    if (!mac) return ESP_FAIL;

    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = kEthPhyAddress;
    phy_cfg.reset_gpio_num = kEthReset;
    esp_eth_phy_t* phy = esp_eth_phy_new_w5500(&phy_cfg);
    if (!phy) return ESP_FAIL;

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_RETURN_ON_ERROR(esp_eth_driver_install(&eth_cfg, &g_eth), kTag, "ETH driver install");

    std::uint8_t mac_addr[6]{};
    ESP_RETURN_ON_ERROR(esp_read_mac(mac_addr, ESP_MAC_ETH), kTag, "ETH MAC read");
    ESP_RETURN_ON_ERROR(esp_eth_ioctl(g_eth, ETH_CMD_S_MAC_ADDR, mac_addr), kTag, "ETH MAC set");

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    g_eth_netif = esp_netif_new(&netif_cfg);
    if (!g_eth_netif) return ESP_ERR_NO_MEM;
    (void)esp_netif_set_hostname(g_eth_netif, "sp01");
    ESP_RETURN_ON_ERROR(esp_netif_attach(g_eth_netif, esp_eth_new_netif_glue(g_eth)), kTag, "ETH netif attach");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event, nullptr), kTag, "ETH event");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_got_ip, nullptr), kTag, "ETH IP event");
    ESP_RETURN_ON_ERROR(esp_eth_start(g_eth), kTag, "ETH start");

    ESP_LOGI(kTag, "W5500 DHCP started: SPI2 SCK15 MOSI13 MISO14 CS16 IRQ12 RST39 hostname=sp01");
    return ESP_OK;
}

esp_err_t start_wifi_optional() noexcept {
    if (!g_config.ssid || g_config.ssid[0] == '\0') {
        ESP_LOGI(kTag, "Wi-Fi STA disabled: SSID empty");
        return ESP_OK;
    }
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_cfg), kTag, "Wi-Fi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event, nullptr), kTag, "Wi-Fi event");
    wifi_config_t sta{};
    std::snprintf(reinterpret_cast<char*>(sta.sta.ssid), sizeof(sta.sta.ssid), "%s", g_config.ssid);
    std::snprintf(reinterpret_cast<char*>(sta.sta.password), sizeof(sta.sta.password), "%s", g_config.password ? g_config.password : "");
    sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), kTag, "Wi-Fi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta), kTag, "Wi-Fi config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), kTag, "Wi-Fi start");
    (void)esp_wifi_set_ps(WIFI_PS_NONE);
    (void)esp_wifi_connect();
    ESP_LOGI(kTag, "Wi-Fi STA connecting to %s", g_config.ssid);
    return ESP_OK;
}

}  // namespace

esp_err_t web_hmi_start(const WebHmiConfig& config,
                        HmiSnapshotFn snapshot_fn,
                        HmiCalZeroFn zero_fn,
                        HmiCalSpanFn span_fn,
                        HmiBenchDoPulseFn bench_do_pulse_fn,
                        HmiBenchDoOffFn bench_do_off_fn) noexcept {
    g_config = config;
    g_snapshot_fn = snapshot_fn;
    g_zero_fn = zero_fn;
    g_span_fn = span_fn;
    g_bench_do_pulse_fn = bench_do_pulse_fn;
    g_bench_do_off_fn = bench_do_off_fn;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    ESP_RETURN_ON_ERROR(start_http(), kTag, "HTTP start");

    const esp_err_t eth_err = start_ethernet();
    if (eth_err != ESP_OK) ESP_LOGE(kTag, "Ethernet disabled: %s", esp_err_to_name(eth_err));

    const esp_err_t wifi_err = start_wifi_optional();
    if (wifi_err != ESP_OK) ESP_LOGW(kTag, "Wi-Fi disabled: %s", esp_err_to_name(wifi_err));

    ESP_LOGI(kTag, "HMI HTTP server ready; Ethernet primary, Wi-Fi optional STA");
    return ESP_OK;
}

}  // namespace sp01
