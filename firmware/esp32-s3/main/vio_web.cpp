// SP01 virtual I/O with embedded web UI.
//
// Provides a browser-driven substitute for bench wiring: eight virtual digital
// inputs the operator can toggle, and eight virtual digital outputs displayed
// as indicators. Intended for remote G2 logic verification when no dry
// contacts, lamps or 24 V supply are available.
//
// SAFETY MODEL
//   1. Compiled only when CONFIG_SP01_VIRTUAL_IO is set. Default is off.
//   2. While virtual I/O is active the TCA9554 physical outputs are written to
//      the safe latch once at startup and are never written again. A virtual DO
//      changes a browser indicator and nothing else.
//   3. The UI states the mode prominently. Virtual readings are never
//      presented as machine readings.
//
// This file contains no control logic. It is an I/O source only.

#include "sp01/vio_web.hpp"

#ifdef CONFIG_SP01_VIRTUAL_IO

#include <cstdio>
#include <cstring>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace sp01 {
namespace {

constexpr char kTag[] = "vio";

httpd_handle_t g_server = nullptr;
volatile uint8_t g_vdi = 0x00;  // bit0 = DI1
volatile uint8_t g_vdo = 0x00;  // bit0 = DO1
char g_ip_text[16] = "0.0.0.0";

// Semantic names from docs/BOARD_TERMINALS.md.
const char* const kDiNames[8] = {
    "hopper.feeder_running", "downstream.conveyor_ready",
    "machine.motor_running", "process.initiative",
    "cycle.fill_position",   "bag.present",
    "position.discharge_ref_a", "position.discharge_ref_b",
};

const char* const kDoNames[8] = {
    "scanner.down",  "bag_detect_air", "bag.push",      "dosing.valve_a",
    "dosing.valve_b", "dosing.valve_c", "filling.motor", "aeration",
};

const char kIndexHtml[] = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport"
content="width=device-width,initial-scale=1"><title>SP01 virtual I/O</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;background:#14161a;color:#e6e8ea}
header{padding:14px 18px;background:#8a2c0d;color:#fff}
header b{font-size:17px}header div{font-size:13px;opacity:.92;margin-top:3px}
main{padding:18px;max-width:900px}
.bar{display:flex;gap:22px;flex-wrap:wrap;font-size:13px;color:#9aa3ad;
margin-bottom:18px}
h2{font-size:14px;text-transform:uppercase;letter-spacing:.09em;color:#9aa3ad;
margin:22px 0 10px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(215px,1fr));
gap:9px}
.row{display:flex;align-items:center;gap:11px;background:#1d2026;
border:1px solid #2a2f37;border-radius:7px;padding:10px 12px}
.led{width:15px;height:15px;border-radius:50%;background:#343a44;flex:none;
border:1px solid #454c58}
.led.on{background:#3ddc6b;border-color:#3ddc6b;box-shadow:0 0 9px #3ddc6b88}
.led.doon{background:#ffb02e;border-color:#ffb02e;box-shadow:0 0 9px #ffb02e88}
.lbl{flex:1;min-width:0}
.lbl b{display:block;font-size:13px}
.lbl span{font-size:11px;color:#828b96;overflow:hidden;text-overflow:ellipsis;
display:block;white-space:nowrap}
button{background:#2b313a;color:#e6e8ea;border:1px solid #3a424e;
border-radius:5px;padding:6px 11px;cursor:pointer;font-size:12px}
button:hover{background:#353d48}
button.on{background:#3ddc6b;color:#0d1117;border-color:#3ddc6b}
button.doon{background:#ffb02e;color:#0d1117;border-color:#ffb02e}
footer{padding:16px 18px;color:#6b7480;font-size:12px}
</style></head><body>
<header><b>SP01 &mdash; VIRTUAL I/O MODE</b>
<div>No physical inputs are read. No physical outputs are driven. The TCA9554
is held in its safe state. Nothing shown here reflects machine condition.</div>
</header>
<main>
<div class="bar"><span id="ip">ip &mdash;</span><span id="up">uptime &mdash;</span>
<span id="heap">heap &mdash;</span><span id="link">link &mdash;</span></div>
<h2>Virtual digital inputs &mdash; click to toggle</h2>
<div class="grid" id="di"></div>
<h2>Virtual digital outputs &mdash; indicator only</h2>
<div class="grid" id="do"></div>
</main>
<footer>SP01 G2 bench aid. Virtual I/O is build-gated and must never be enabled
on a machine-connected node.</footer>
<script>
const DIN=["hopper.feeder_running","downstream.conveyor_ready",
"machine.motor_running","process.initiative","cycle.fill_position",
"bag.present","position.discharge_ref_a","position.discharge_ref_b"];
const DON=["scanner.down","bag_detect_air","bag.push","dosing.valve_a",
"dosing.valve_b","dosing.valve_c","filling.motor","aeration"];
function build(){
 const di=document.getElementById('di'),dq=document.getElementById('do');
 for(let i=0;i<8;i++){
  const r=document.createElement('div');r.className='row';
  r.innerHTML='<div class="led" id="dl'+i+'"></div><div class="lbl"><b>DI'+
  (i+1)+'</b><span>'+DIN[i]+'</span></div>';
  const b=document.createElement('button');b.textContent='OPEN';b.id='db'+i;
  b.onclick=()=>toggle(i);r.appendChild(b);di.appendChild(r);
  const s=document.createElement('div');s.className='row';
  s.innerHTML='<div class="led" id="ol'+i+'"></div><div class="lbl"><b>DO'+
  (i+1)+'</b><span>'+DON[i]+'</span></div><button id="ob'+i+
  '">OFF</button>';dq.appendChild(s);
  s.querySelector('#ob'+i).onclick=()=>toggleDo(i);
 }
}
async function toggle(i){
 const on=document.getElementById('db'+i).textContent==='CLOSED';
 await fetch('/api/di?ch='+i+'&v='+(on?0:1),{method:'POST'});refresh();
}
async function toggleDo(i){
 const on=document.getElementById('ob'+i).textContent==='ON';
 await fetch('/api/do?ch='+i+'&v='+(on?0:1),{method:'POST'});refresh();
}
async function refresh(){
 try{
  const r=await fetch('/api/io');const j=await r.json();
  document.getElementById('ip').textContent='ip '+j.ip;
  document.getElementById('up').textContent='uptime '+
   Math.floor(j.up/1000)+' s';
  document.getElementById('heap').textContent='heap '+j.heap;
  document.getElementById('link').textContent='link '+(j.link?'UP':'down');
  for(let i=0;i<8;i++){
   const d=(j.di>>i)&1,o=(j.do>>i)&1;
   const dl=document.getElementById('dl'+i),db=document.getElementById('db'+i);
   dl.className='led'+(d?' on':'');db.textContent=d?'CLOSED':'OPEN';
   db.className=d?'on':'';
   const ol=document.getElementById('ol'+i),ob=document.getElementById('ob'+i);
   ol.className='led'+(o?' doon':'');ob.textContent=o?'ON':'OFF';
   ob.className=o?'doon':'';
  }
 }catch(e){document.getElementById('link').textContent='link OFFLINE';}
}
build();refresh();setInterval(refresh,500);
</script></body></html>)HTML";

esp_err_t index_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t io_handler(httpd_req_t* req) {
    char body[192];
    const int n = std::snprintf(
        body, sizeof(body),
        "{\"mode\":\"virtual\",\"di\":%u,\"do\":%u,\"ip\":\"%s\",\"up\":%llu,"
        "\"heap\":%lu,\"link\":%s}",
        static_cast<unsigned>(g_vdi), static_cast<unsigned>(g_vdo), g_ip_text,
        static_cast<unsigned long long>(esp_timer_get_time() / 1000),
        static_cast<unsigned long>(esp_get_free_heap_size()),
        vio_link_up() ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, n);
}

bool parse_ch_val(httpd_req_t* req, int* ch, int* val) {
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
        return false;
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
    g_vdi = val ? (g_vdi | mask) : (g_vdi & static_cast<uint8_t>(~mask));
    ESP_LOGI(kTag, "virtual DI%d %-26s -> %s", ch + 1, kDiNames[ch],
             val ? "CLOSED" : "OPEN");
    return httpd_resp_sendstr(req, "ok");
}

esp_err_t do_handler(httpd_req_t* req) {
    int ch = 0, val = 0;
    if (!parse_ch_val(req, &ch, &val)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ch=0..7 & v=0|1");
        return ESP_FAIL;
    }
    const uint8_t mask = static_cast<uint8_t>(1u << ch);
    g_vdo = val ? (g_vdo | mask) : (g_vdo & static_cast<uint8_t>(~mask));
    ESP_LOGI(kTag, "virtual DO%d %-26s -> %s  (no physical output driven)",
             ch + 1, kDoNames[ch], val ? "ON" : "OFF");
    return httpd_resp_sendstr(req, "ok");
}

}  // namespace

void vio_set_ip(const char* ip) {
    std::snprintf(g_ip_text, sizeof(g_ip_text), "%s", ip);
}

uint8_t vio_di() { return g_vdi; }
uint8_t vio_do() { return g_vdo; }

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
        {"/api/do", HTTP_POST, do_handler, nullptr},
    };
    for (const auto& r : routes) {
        err = httpd_register_uri_handler(g_server, &r);
        if (err != ESP_OK) return err;
    }

    ESP_LOGW(kTag, "VIRTUAL I/O ACTIVE - no physical inputs read,");
    ESP_LOGW(kTag, "no physical outputs driven, TCA9554 held safe.");
    ESP_LOGI(kTag, "web UI on http://%s/", g_ip_text);
    return ESP_OK;
}

}  // namespace sp01

#endif  // CONFIG_SP01_VIRTUAL_IO
