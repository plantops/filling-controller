// SP01 virtual I/O with embedded web UI.
//
// Browser-driven virtual DI plus controller-derived virtual DO. This build is
// commissioning-only: physical TCA9554 outputs remain in the safe state. The
// broken-bag controls below alter only the simulated weight/position inputs so
// G3 can exercise the real C++ controller without inventing machine wiring.

#include "sp01/vio_web.hpp"

#ifdef CONFIG_SP01_VIRTUAL_IO

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace sp01 {
namespace {

constexpr char kTag[] = "vio";

httpd_handle_t g_server = nullptr;
uint8_t g_vdi = 0x00;
char g_ip_text[16] = "0.0.0.0";
portMUX_TYPE g_vio_mux = portMUX_INITIALIZER_UNLOCKED;
VioStatus g_status{};
VioCommand g_command = VioCommand::None;

const char* const kDiNames[8] = {
    "hopper.feeder_running", "downstream.conveyor_ready",
    "machine.motor_running", "process.initiative",
    "cycle.fill_position", "bag.present",
    "position.discharge_ref_a", "position.discharge_ref_b",
};

const char kIndexHtml[] = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SP01 virtual bench</title>
<style>
:root{color-scheme:dark}*{box-sizing:border-box}body{font-family:system-ui,sans-serif;margin:0;background:#14161a;color:#e6e8ea}header{padding:13px 18px;background:#8a2c0d;color:#fff}header b{font-size:17px}header div{font-size:13px;opacity:.92;margin-top:3px}main{padding:18px;max-width:1100px;margin:auto}.bar,.acts{display:flex;gap:10px;flex-wrap:wrap;align-items:center}.bar{font-size:12px;color:#9aa3ad;margin-bottom:14px}.fsm{display:flex;gap:10px;flex-wrap:wrap;align-items:center;background:#1d2026;border:1px solid #2a2f37;border-radius:8px;padding:14px 16px}.st{font-size:22px;font-weight:650}.pill{font-size:12px;padding:3px 9px;border-radius:99px;background:#2b313a;border:1px solid #3a424e}.bad{background:#7a1d1d;border-color:#a33;color:#ffdede}.reject{background:#6d3d00;border-color:#a86c16}.wt{margin-left:auto;font-variant-numeric:tabular-nums;font-size:22px;font-weight:650}.wrap{height:9px;background:#242932;border-radius:5px;overflow:hidden;margin:10px 0 16px}.fill{height:100%;width:0;background:#3ddc6b;transition:width .25s}h2{font-size:13px;text-transform:uppercase;letter-spacing:.09em;color:#9aa3ad;margin:20px 0 9px}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(215px,1fr));gap:8px}.row{display:flex;align-items:center;gap:10px;background:#1d2026;border:1px solid #2a2f37;border-radius:7px;padding:9px 11px}.led{width:14px;height:14px;border-radius:50%;background:#343a44;border:1px solid #454c58;flex:none}.led.on{background:#3ddc6b;border-color:#3ddc6b;box-shadow:0 0 8px #3ddc6b88}.led.doon{background:#ffb02e;border-color:#ffb02e;box-shadow:0 0 8px #ffb02e88}.lbl{flex:1;min-width:0}.lbl b{display:block;font-size:13px}.lbl span{font-size:11px;color:#828b96;overflow:hidden;text-overflow:ellipsis;display:block;white-space:nowrap}button{background:#2b313a;color:#e6e8ea;border:1px solid #3a424e;border-radius:5px;padding:7px 11px;cursor:pointer;font-size:12px}button:hover{background:#353d48}button.on{background:#3ddc6b;color:#0d1117;border-color:#3ddc6b}.danger{border-color:#a86c16}.note{font-size:12px;color:#9aa3ad;margin-top:8px}footer{padding:16px 18px;color:#6b7480;font-size:12px;max-width:1100px;margin:auto}
</style></head><body>
<header><b>SP01 — VIRTUAL BENCH</b><div>Virtual DI + simulated weight + semantic reject window. No physical output authority. Simulation thresholds are not production values.</div></header>
<main>
<div class="bar"><span id="ip">ip —</span><span id="up">uptime —</span><span id="heap">heap —</span><span id="link">link —</span><span id="broken">loss sim OFF</span></div>
<div class="fsm"><span class="st" id="state">—</span><span class="pill" id="mode">—</span><span class="pill" id="fault">—</span><span class="pill" id="disp">UNDECIDED</span><span class="pill" id="cycle">cycle —</span><span class="wt" id="wt">0.000 kg</span></div>
<div class="wrap"><div class="fill" id="bar"></div></div>
<div class="acts"><button onclick="cmd('reset')">Reset</button><button onclick="cmd('clear')">Clear fault</button><button class="danger" onclick="cmd('break_on')">Broken-bag loss ON</button><button onclick="cmd('break_off')">Loss OFF</button><button class="danger" onclick="cmd('reject210')">Pulse ~210° reject window</button></div>
<div class="note">G3 reject test: enter COARSE/FINE, turn loss ON, observe REJECT_WAIT and DO4..DO8 OFF, then pulse ~210° window and observe DO3 only. This does not prove the real machine 210° reference.</div>
<h2>Virtual digital inputs — click to toggle</h2><div class="grid" id="di"></div>
<h2>Controller digital outputs — virtual indicators only</h2><div class="grid" id="do"></div>
</main><footer>Runs the real SP01 controller against virtual inputs. Build-gated; never enable this virtual-I/O build on a machine-connected controller.</footer>
<script>
const DIN=["hopper.feeder_running","downstream.conveyor_ready","machine.motor_running","process.initiative","cycle.fill_position","bag.present","position.discharge_ref_a","position.discharge_ref_b"];
const DON=["scanner.down","bag_detect_air","bag.push","dosing.valve_a","dosing.valve_b","dosing.valve_c","filling.motor","spout.aeration"];
function build(){const a=document.getElementById('di'),b=document.getElementById('do');for(let i=0;i<8;i++){const r=document.createElement('div');r.className='row';r.innerHTML='<div class="led" id="dl'+i+'"></div><div class="lbl"><b>DI'+(i+1)+'</b><span>'+DIN[i]+'</span></div>';const x=document.createElement('button');x.textContent='OPEN';x.id='db'+i;x.onclick=()=>toggle(i);r.appendChild(x);a.appendChild(r);const q=document.createElement('div');q.className='row';q.innerHTML='<div class="led" id="ol'+i+'"></div><div class="lbl"><b>DO'+(i+1)+'</b><span>'+DON[i]+'</span></div>';b.appendChild(q)}}
async function toggle(i){const on=document.getElementById('db'+i).textContent==='CLOSED';await fetch('/api/di?ch='+i+'&v='+(on?0:1),{method:'POST'});refresh()}
async function cmd(a){await fetch('/api/cmd?a='+a,{method:'POST'});refresh()}
async function refresh(){try{const j=await(await fetch('/api/io',{cache:'no-store'})).json();ip.textContent='ip '+j.ip;up.textContent='uptime '+Math.floor(j.up/1000)+' s';heap.textContent='heap '+j.heap;link.textContent='link '+(j.link?'UP':'down');broken.textContent='loss sim '+(j.broken_sim?'ON':'OFF');state.textContent=j.state;mode.textContent=j.opmode;fault.textContent='fault '+j.fault;fault.className='pill'+(j.fault!=='NONE'?' bad':'');disp.textContent=j.disposition;disp.className='pill'+(j.disposition==='REJECT'?' reject':'');cycle.textContent='cycle '+j.cycle;wt.textContent=j.weight.toFixed(3)+' kg';bar.style.width=Math.min(100,100*j.weight/(j.target||1))+'%';for(let i=0;i<8;i++){const d=(j.di>>i)&1,o=(j.do>>i)&1;document.getElementById('dl'+i).className='led'+(d?' on':'');const db=document.getElementById('db'+i);db.textContent=d?'CLOSED':'OPEN';db.className=d?'on':'';document.getElementById('ol'+i).className='led'+(o?' doon':'')}}catch(e){link.textContent='link OFFLINE'}}
build();refresh();setInterval(refresh,300);
</script></body></html>)HTML";

esp_err_t index_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t io_handler(httpd_req_t* req) {
    uint8_t vdi = 0;
    VioStatus status{};
    char ip[sizeof(g_ip_text)]{};
    portENTER_CRITICAL(&g_vio_mux);
    vdi = g_vdi;
    status = g_status;
    std::memcpy(ip, g_ip_text, sizeof(ip));
    portEXIT_CRITICAL(&g_vio_mux);

    char body[640];
    const int n = std::snprintf(
        body, sizeof(body),
        "{\"mode\":\"virtual\",\"di\":%u,\"do\":%u,\"ip\":\"%s\","
        "\"up\":%llu,\"heap\":%lu,\"link\":%s,\"state\":\"%s\","
        "\"fault\":\"%s\",\"opmode\":\"%s\",\"disposition\":\"%s\","
        "\"weight\":%.3f,\"target\":%.2f,\"cycle\":%lu,"
        "\"broken_sim\":%s,\"reject_window\":%s,\"broken_detected_us\":%llu}",
        static_cast<unsigned>(vdi), static_cast<unsigned>(status.do_bits), ip,
        static_cast<unsigned long long>(esp_timer_get_time() / 1000),
        static_cast<unsigned long>(esp_get_free_heap_size()),
        vio_link_up() ? "true" : "false", status.state, status.fault,
        status.mode, status.disposition, static_cast<double>(status.weight_kg),
        static_cast<double>(status.target_kg), static_cast<unsigned long>(status.cycle_id),
        status.broken_bag_sim ? "true" : "false",
        status.reject_window ? "true" : "false",
        static_cast<unsigned long long>(status.broken_bag_detected_us));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, n);
}

bool parse_ch_val(httpd_req_t* req, int* ch, int* val) {
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) return false;
    char a[8], b[8];
    if (httpd_query_key_value(query, "ch", a, sizeof(a)) != ESP_OK) return false;
    if (httpd_query_key_value(query, "v", b, sizeof(b)) != ESP_OK) return false;
    *ch = atoi(a);
    *val = atoi(b);
    return (*ch >= 0 && *ch < 8);
}

esp_err_t di_handler(httpd_req_t* req) {
    int ch = 0, val = 0;
    if (!parse_ch_val(req, &ch, &val)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ch=0..7 & v=0|1");
        return ESP_FAIL;
    }
    const uint8_t mask = static_cast<uint8_t>(1u << ch);
    portENTER_CRITICAL(&g_vio_mux);
    g_vdi = val ? static_cast<uint8_t>(g_vdi | mask)
                : static_cast<uint8_t>(g_vdi & static_cast<uint8_t>(~mask));
    portEXIT_CRITICAL(&g_vio_mux);
    ESP_LOGI(kTag, "virtual DI%d %-26s -> %s", ch + 1, kDiNames[ch],
             val ? "CLOSED" : "OPEN");
    return httpd_resp_sendstr(req, "ok");
}

esp_err_t cmd_handler(httpd_req_t* req) {
    char query[64];
    char action[20];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "a", action, sizeof(action)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "a=reset|clear|break_on|break_off|reject210");
        return ESP_FAIL;
    }

    VioCommand next = VioCommand::None;
    if (std::strcmp(action, "reset") == 0) next = VioCommand::Reset;
    else if (std::strcmp(action, "clear") == 0) next = VioCommand::ClearFault;
    else if (std::strcmp(action, "break_on") == 0) next = VioCommand::BrokenBagOn;
    else if (std::strcmp(action, "break_off") == 0) next = VioCommand::BrokenBagOff;
    else if (std::strcmp(action, "reject210") == 0) next = VioCommand::RejectWindow;
    else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "a=reset|clear|break_on|break_off|reject210");
        return ESP_FAIL;
    }

    portENTER_CRITICAL(&g_vio_mux);
    g_command = next;
    portEXIT_CRITICAL(&g_vio_mux);
    ESP_LOGI(kTag, "operator command: %s", action);
    return httpd_resp_sendstr(req, "ok");
}

}  // namespace

void vio_set_ip(const char* ip) {
    portENTER_CRITICAL(&g_vio_mux);
    std::snprintf(g_ip_text, sizeof(g_ip_text), "%s", ip);
    portEXIT_CRITICAL(&g_vio_mux);
}

uint8_t vio_di() {
    portENTER_CRITICAL(&g_vio_mux);
    const uint8_t copy = g_vdi;
    portEXIT_CRITICAL(&g_vio_mux);
    return copy;
}

void vio_publish(const VioStatus& status) {
    portENTER_CRITICAL(&g_vio_mux);
    g_status = status;
    portEXIT_CRITICAL(&g_vio_mux);
}

VioCommand vio_take_command() {
    portENTER_CRITICAL(&g_vio_mux);
    const VioCommand c = g_command;
    g_command = VioCommand::None;
    portEXIT_CRITICAL(&g_vio_mux);
    return c;
}

esp_err_t vio_start() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 8;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 6144;

    esp_err_t err = httpd_start(&g_server, &cfg);
    if (err != ESP_OK) return err;

    const httpd_uri_t routes[] = {
        {"/", HTTP_GET, index_handler, nullptr},
        {"/api/io", HTTP_GET, io_handler, nullptr},
        {"/api/di", HTTP_POST, di_handler, nullptr},
        {"/api/cmd", HTTP_POST, cmd_handler, nullptr},
    };
    for (const auto& r : routes) {
        err = httpd_register_uri_handler(g_server, &r);
        if (err != ESP_OK) return err;
    }

    char ip[sizeof(g_ip_text)]{};
    portENTER_CRITICAL(&g_vio_mux);
    std::memcpy(ip, g_ip_text, sizeof(ip));
    portEXIT_CRITICAL(&g_vio_mux);
    ESP_LOGW(kTag, "VIRTUAL I/O ACTIVE - no physical inputs read,");
    ESP_LOGW(kTag, "no physical outputs driven, TCA9554 held safe.");
    ESP_LOGI(kTag, "web UI on http://%s/", ip);
    return ESP_OK;
}

}  // namespace sp01

#endif  // CONFIG_SP01_VIRTUAL_IO
