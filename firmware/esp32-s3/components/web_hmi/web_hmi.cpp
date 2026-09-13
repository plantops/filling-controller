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
#include "esp_timer.h"
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

constexpr char kOpHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SP01 Operator HMI</title>
<style>
:root{color-scheme:dark}body{font-family:system-ui,sans-serif;margin:0;background:#101214;color:#eef1f4}.wrap{max-width:820px;margin:auto;padding:18px}.top{display:flex;justify-content:space-between;align-items:center;gap:12px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin-top:16px}.card{background:#181c20;border:1px solid #343b42;border-radius:12px;padding:16px}.k{font-size:12px;opacity:.65;text-transform:uppercase}.v{font-size:26px;font-weight:700;margin-top:5px}.ok{color:#9be28f}.bad{color:#ff9b9b}.muted{opacity:.65}a{color:#9fc7ff;text-decoration:none}.status{font-weight:700}
</style></head><body><div class="wrap">
<div class="top"><div><h2 style="margin:0">SP01 Filling Controller</h2><div class="muted">Operator view</div></div><a href="/dev">DEV MODE →</a></div>
<div id="conn" class="status">Connecting…</div>
<div class="grid">
<div class="card"><div class="k">State</div><div id="state" class="v">-</div></div>
<div class="card"><div class="k">Weight</div><div id="weight" class="v">-</div></div>
<div class="card"><div class="k">Bag</div><div id="bag" class="v">-</div></div>
<div class="card"><div class="k">Machine ready</div><div id="ready" class="v">-</div></div>
<div class="card"><div class="k">Fill</div><div id="fill" class="v">-</div></div>
<div class="card"><div class="k">Fault</div><div id="fault" class="v">-</div></div>
</div></div>
<script>
function onBit(v,n){return !!(v&(1<<n))}
async function poll(){try{const r=await fetch('/api/state',{cache:'no-store'});if(!r.ok)throw 0;const s=await r.json();conn.textContent='LIVE · '+s.mode+' · cycle '+s.cycle_id;conn.className='status ok';state.textContent=s.state;weight.textContent=s.weight.toFixed(2)+' kg';bag.textContent=s.disposition;const perm=onBit(s.di,0)&&onBit(s.di,1)&&onBit(s.di,2)&&onBit(s.di,3);ready.textContent=perm?'READY':'WAIT';ready.className='v '+(perm?'ok':'');const filling=(s.desired_do&(1<<6))!==0;fill.textContent=filling?'FILLING':'IDLE';fault.textContent=s.fault;fault.className='v '+(s.fault==='NONE'?'ok':'bad');}catch(e){conn.textContent='OFFLINE';conn.className='status bad'}setTimeout(poll,350)}poll();
</script></body></html>)HTML";

constexpr char kDevHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SP01 DEV HMI</title>
<style>
:root{color-scheme:dark}*{box-sizing:border-box}body{font-family:system-ui,sans-serif;margin:0;background:#0d1013;color:#e9edf1}.wrap{max-width:1500px;margin:auto;padding:12px}.head{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}.summary{display:flex;gap:8px;flex-wrap:wrap}.pill{border:1px solid #3a434c;border-radius:999px;padding:6px 10px;background:#161b20}.grid3{display:grid;grid-template-columns:1fr 1.15fr 1fr;gap:10px;margin-top:10px}.card{border:1px solid #343c44;border-radius:10px;background:#15191e;padding:12px}.card h3{font-size:14px;margin:0 0 10px;color:#cfd6dc}.row{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:5px 0;border-bottom:1px solid #252c32}.row:last-child{border-bottom:0}.lamp{min-width:48px;text-align:center;border-radius:6px;padding:3px 6px;background:#252b31;color:#aeb6bd;font-size:12px;font-weight:700}.lamp.on{background:#1e5a34;color:#b9ffc9}.lamp.warn{background:#6b4b12;color:#ffe0a0}.lamp.bad{background:#6d2525;color:#ffc1c1}.flow{display:flex;gap:5px;overflow:auto;padding:4px 0}.st{border:1px solid #3a424a;border-radius:7px;padding:7px 9px;white-space:nowrap;font-size:12px;opacity:.55}.st.active{opacity:1;border-color:#9fc7ff;background:#183149;color:#d8ecff}.branch{margin-top:7px;font-size:12px;opacity:.75}.mono{font-family:ui-monospace,SFMono-Regular,Consolas,monospace}.small{font-size:12px;opacity:.7}.big{font-size:22px;font-weight:700}.ok{color:#9be28f}.badText{color:#ff9b9b}.warnText{color:#ffd28a}.two{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:10px}button{font:inherit;border:1px solid #4a555f;background:#20262c;color:#eef;border-radius:7px;padding:7px 10px;cursor:pointer}button:hover{background:#29313a}a{color:#9fc7ff;text-decoration:none}@media(max-width:980px){.grid3,.two{grid-template-columns:1fr}}
</style></head><body><div class="wrap">
<div class="head"><div><h2 style="margin:0">SP01 · DEV MODE</h2><div class="small">Core controller design / debug / tuning. Input source = REAL HW; this page does not force raw pins.</div></div><div><button id="freeze" onclick="toggleFreeze()">Freeze</button> <a href="/">OP MODE →</a></div></div>
<div class="summary" style="margin-top:10px"><span class="pill">Conn <b id="conn">…</b></span><span class="pill">Mode <b id="mode">-</b></span><span class="pill">State <b id="state">-</b></span><span class="pill">Cycle <b id="cycle">-</b></span><span class="pill">Bag <b id="disp">-</b></span><span class="pill">Weight <b id="weight">-</b></span><span class="pill">Fault <b id="fault">-</b></span></div>

<div class="grid3">
<div class="card"><h3>A · ACTUAL HW INPUTS</h3><div id="inputs"></div><div class="small" style="margin-top:8px">Semantic DI state read from board after configured polarity. No GPIO/register detail here.</div></div>
<div class="card"><h3>B · CONTROLLER CORE</h3><div class="row"><span>Interlocks ready</span><span id="perm" class="lamp">-</span></div><div class="row"><span>Weight quality</span><span id="wq" class="lamp">-</span></div><div class="row"><span>Weight stable</span><span id="ws" class="lamp">-</span></div><div class="row"><span>Broken bag detected</span><span id="broken" class="lamp">-</span></div><div class="row"><span>Reject latched</span><span id="reject" class="lamp">-</span></div><div class="row"><span>Next transition</span><b id="next">-</b></div><div class="row"><span>Block / wait reason</span><b id="block">-</b></div><div class="row"><span>Core output map</span><b id="corecheck">-</b></div></div>
<div class="card"><h3>C · OUTPUTS</h3><div id="outputs"></div><div class="small" style="margin-top:8px"><b>Desired</b> = FSM request. <b>Commanded</b> = last board-adapter command. No independent physical DO feedback is claimed.</div></div>
</div>

<div class="card" style="margin-top:10px"><h3>D · EXECUTABLE STATE FLOW</h3><div id="flow" class="flow"></div><div class="branch"><b>GOOD:</b> SETTLE → WAIT_DISCHARGE → PUSH → COMPLETE &nbsp; | &nbsp; <b>REJECT:</b> COARSE/FINE → REJECT_WAIT → PUSH → COMPLETE</div></div>

<div class="two">
<div class="card"><h3>E · SLOW MANUAL TEST</h3><div class="small">Toggle real semantic inputs slowly and watch the controller advance. Do not test raw pins individually here.</div><ol style="margin:8px 0 0 18px;padding:0"><li>Permissives: feeder + downstream + motor + initiative.</li><li>Cycle fill-position OFF → ON.</li><li>Bag present ON.</li><li>Watch BAG_ACQUIRE → BAG_VERIFY → TARE_READY.</li><li>Dummy weight drives COARSE_FILL → FINE_FILL → CUTOFF → SETTLE.</li><li>GOOD path: exercise ref A then ref B; verify PUSH only after discharge timing.</li><li>REJECT path remains dependent on the commissioned reject-window authority; do not invent a 210° raw DI.</li></ol></div>
<div class="card"><h3>F · EVENT TIMELINE</h3><div id="timeline" class="mono small">waiting…</div></div>
</div>

<div class="two">
<div class="card"><h3>WEIGHT / REJECT</h3><div class="row"><span>Net</span><b id="wnet">-</b></div><div class="row"><span>Sample sequence</span><b id="wseq">-</b></div><div class="row"><span>Sample age</span><b id="wage">-</b></div><div class="row"><span>Broken peak</span><b id="bpeak">-</b></div><div class="row"><span>Broken weight</span><b id="bweight">-</b></div></div>
<div class="card"><h3>DISCHARGE TIMING</h3><div class="row"><span>Ref A→B interval</span><b id="refint">-</b></div><div class="row"><span>Push due (controller us)</span><b id="due">-</b></div><div class="row"><span>Desired mask</span><b id="dmask" class="mono">-</b></div><div class="row"><span>Commanded mask</span><b id="cmask" class="mono">-</b></div></div>
</div>
</div>
<script>
const DI=['feeder_running','downstream_ready','machine_motor_running','initiative','fill_position','bag_present','discharge_ref_a','discharge_ref_b'];
const DO=['scanner.down','bag_detect_air','bag.push','dosing.valve_a','dosing.valve_b','dosing.valve_c','filling.motor','spout.aeration'];
const STATES=['WAIT_PERMISSIVE','WAIT_FILL_POSITION','BAG_ACQUIRE','BAG_VERIFY','TARE_READY','COARSE_FILL','FINE_FILL','CUTOFF','SETTLE','REJECT_WAIT','WAIT_DISCHARGE','PUSH','COMPLETE','FAULT'];
let frozen=false,last=null,lastState='',events=[];
function bit(v,n){return !!(v&(1<<n))}
function lamp(v){return `<span class="lamp ${v?'on':''}">${v?'ON':'OFF'}</span>`}
function expectedMask(s){switch(s.state){case'BAG_ACQUIRE':case'BAG_VERIFY':case'TARE_READY':return 0x03;case'COARSE_FILL':return 0xfb;case'FINE_FILL':return 0xeb;case'CUTOFF':case'SETTLE':case'REJECT_WAIT':case'WAIT_DISCHARGE':return 0x03;case'PUSH':return s.mode==='AUTO'?0x04:0;default:return 0}}
function readiness(s){if(s.mode==='AUTO')return bit(s.di,0)&&bit(s.di,1)&&bit(s.di,2)&&bit(s.di,3);return bit(s.di,0)&&bit(s.di,3)}
function nextInfo(s){let n='-',b='-';switch(s.state){case'WAIT_PERMISSIVE':n=s.mode==='AUTO'?'WAIT_FILL_POSITION':'BAG_ACQUIRE';b=readiness(s)?'ready to transition':'waiting permissives';break;case'WAIT_FILL_POSITION':n='BAG_ACQUIRE';b='waiting fill_position OFF → ON edge';break;case'BAG_ACQUIRE':n='BAG_VERIFY';b=bit(s.di,5)?'bag detected':'waiting bag_present';break;case'BAG_VERIFY':n='TARE_READY';b='verify bag';break;case'TARE_READY':n='COARSE_FILL';b=s.quality===1?'weight ready':'waiting healthy fresh weight';break;case'COARSE_FILL':n='FINE_FILL or REJECT_WAIT';b='weight threshold / broken-bag watch';break;case'FINE_FILL':n='CUTOFF or REJECT_WAIT';b='cutoff threshold / broken-bag watch';break;case'CUTOFF':n='SETTLE';b='immediate transition';break;case'SETTLE':n=s.mode==='AUTO'?'WAIT_DISCHARGE':'COMPLETE';b=s.stable?'stable':'waiting stable weight';break;case'REJECT_WAIT':n='PUSH';b='waiting commissioned reject-window authority';break;case'WAIT_DISCHARGE':n='PUSH';b=s.discharge_due_us?'waiting due time':'waiting ref A → ref B';break;case'PUSH':n='COMPLETE';b='push pulse duration';break;case'COMPLETE':n=s.mode==='AUTO'?'WAIT_FILL_POSITION / WAIT_PERMISSIVE':'WAIT_PERMISSIVE';b='cycle complete';break;case'FAULT':n='clear fault';b=s.fault;break}return[n,b]}
function renderFlow(state){flow.innerHTML=STATES.map(x=>`<span class="st ${x===state?'active':''}">${x}</span>`).join('')}
function render(s){last=s;conn.textContent='LIVE';conn.className='ok';mode.textContent=s.mode;state.textContent=s.state;cycle.textContent=s.cycle_id;disp.textContent=s.disposition;weight.textContent=s.weight.toFixed(2)+' kg';fault.textContent=s.fault;fault.className=s.fault==='NONE'?'ok':'badText';inputs.innerHTML=DI.map((n,i)=>`<div class="row"><span>DI${i+1} · ${n}</span>${lamp(bit(s.di,i))}</div>`).join('');outputs.innerHTML=DO.map((n,i)=>{const d=bit(s.desired_do,i),c=bit(s.commanded_do,i);return `<div class="row"><span>DO${i+1} · ${n}</span><span><span class="lamp ${d?'warn':''}">D:${d?'ON':'OFF'}</span> <span class="lamp ${c?'on':''}">C:${c?'ON':'OFF'}</span></span></div>`}).join('');const p=readiness(s);perm.textContent=p?'YES':'NO';perm.className='lamp '+(p?'on':'');wq.textContent=['UNKNOWN','GOOD','STALE','FAULT'][s.quality]||s.quality;wq.className='lamp '+(s.quality===1?'on':s.quality===3?'bad':'warn');ws.textContent=s.stable?'YES':'NO';ws.className='lamp '+(s.stable?'on':'');const br=s.broken_bag_detected_us>0;broken.textContent=br?'YES':'NO';broken.className='lamp '+(br?'bad':'');const rj=s.disposition==='REJECT';reject.textContent=rj?'YES':'NO';reject.className='lamp '+(rj?'bad':'');const ni=nextInfo(s);next.textContent=ni[0];block.textContent=ni[1];const exp=expectedMask(s),mapok=(s.desired_do&255)===exp;corecheck.textContent=mapok?'OK':'MISMATCH expected 0x'+exp.toString(16).padStart(2,'0');corecheck.className=mapok?'ok':'badText';renderFlow(s.state);wnet.textContent=s.weight.toFixed(3)+' kg';wseq.textContent=s.weight_sequence;wage.textContent=s.weight_age_ms+' ms';bpeak.textContent=s.broken_bag_peak_kg.toFixed(3)+' kg';bweight.textContent=s.broken_bag_weight_kg.toFixed(3)+' kg';refint.textContent=s.discharge_ref_interval_us+' us';due.textContent=s.discharge_due_us;dmask.textContent='0x'+(s.desired_do&255).toString(16).padStart(2,'0');cmask.textContent='0x'+(s.commanded_do&255).toString(16).padStart(2,'0');if(s.state!==lastState){const t=new Date().toLocaleTimeString();events.unshift(`${t}  ${lastState||'BOOT'} → ${s.state}  fault=${s.fault} bag=${s.disposition}`);events=events.slice(0,12);timeline.textContent=events.join('\n');lastState=s.state}}
function toggleFreeze(){frozen=!frozen;freeze.textContent=frozen?'Resume':'Freeze';if(!frozen&&last)render(last)}
async function poll(){if(!frozen){try{const r=await fetch('/api/state',{cache:'no-store'});if(!r.ok)throw 0;render(await r.json())}catch(e){conn.textContent='OFFLINE';conn.className='badText'}}setTimeout(poll,200)}poll();
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
    return send_text(req, kOpHtml, "text/html");
}

esp_err_t dev_handler(httpd_req_t* req) noexcept {
    return send_text(req, kDevHtml, "text/html");
}

esp_err_t state_handler(httpd_req_t* req) noexcept {
    if (!g_snapshot_fn) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "status unavailable");
    HmiSnapshot s{};
    if (!g_snapshot_fn(s)) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "status unavailable");
    const auto now_us = static_cast<std::uint64_t>(esp_timer_get_time());
    const std::uint64_t age_ms = (s.weight.sample_time_us <= now_us)
                                     ? (now_us - s.weight.sample_time_us) / 1000ULL
                                     : 0ULL;
    char json[1536]{};
    std::snprintf(json, sizeof(json),
                  "{\"mode\":\"%s\",\"state\":\"%s\",\"fault\":\"%s\",\"disposition\":\"%s\","
                  "\"cycle\":%" PRIu32 ",\"cycle_id\":%" PRIu32 ",\"weight\":%.3f,\"stable\":%s,\"quality\":%u,"
                  "\"di\":%u,\"do\":%u,\"desired_do\":%u,\"commanded_do\":%u,"
                  "\"broken_bag_detected_us\":%" PRIu64 ",\"broken_bag_peak_kg\":%.3f,\"broken_bag_weight_kg\":%.3f,"
                  "\"discharge_ref_interval_us\":%" PRIu64 ",\"discharge_due_us\":%" PRIu64 ","
                  "\"weight_sequence\":%" PRIu32 ",\"weight_sample_time_us\":%" PRIu64 ",\"weight_age_ms\":%" PRIu64 ","
                  "\"service_ready\":%s,\"bench_do_available\":%s,"
                  "\"tlb_polls\":%" PRIu32 ",\"tlb_errors\":%" PRIu32 ",\"tlb_last_error\":%d}",
                  mode_name(s.controller.mode), state_name(s.controller.state), fault_name(s.controller.fault),
                  disposition_name(s.controller.disposition), s.controller.cycle_id, s.controller.cycle_id,
                  static_cast<double>(s.weight.net_kg), s.weight.stable ? "true" : "false",
                  static_cast<unsigned>(s.weight.quality), static_cast<unsigned>(pack_inputs(s.inputs)),
                  static_cast<unsigned>(pack_outputs(s.commanded_outputs)),
                  static_cast<unsigned>(pack_outputs(s.controller.outputs)),
                  static_cast<unsigned>(pack_outputs(s.commanded_outputs)),
                  s.controller.broken_bag_detected_us, static_cast<double>(s.controller.broken_bag_peak_kg),
                  static_cast<double>(s.controller.broken_bag_weight_kg), s.controller.discharge_ref_interval_us,
                  s.controller.discharge_due_us, s.weight.sequence, s.weight.sample_time_us, age_ms,
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
    if (!token_ok(req)) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "bad service token");
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
    cfg.max_uri_handlers = 8;
    esp_err_t err = httpd_start(&g_server, &cfg);
    if (err != ESP_OK) return err;

    httpd_uri_t root{}; root.uri = "/"; root.method = HTTP_GET; root.handler = root_handler;
    httpd_uri_t dev{}; dev.uri = "/dev"; dev.method = HTTP_GET; dev.handler = dev_handler;
    httpd_uri_t state{}; state.uri = "/api/state"; state.method = HTTP_GET; state.handler = state_handler;
    httpd_uri_t zero{}; zero.uri = "/api/cal/zero"; zero.method = HTTP_POST; zero.handler = zero_handler;
    httpd_uri_t span{}; span.uri = "/api/cal/span"; span.method = HTTP_POST; span.handler = span_handler;
    httpd_uri_t bench{}; bench.uri = "/api/bench/do"; bench.method = HTTP_POST; bench.handler = bench_do_handler;
    httpd_register_uri_handler(g_server, &root);
    httpd_register_uri_handler(g_server, &dev);
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

    ESP_LOGI(kTag, "HMI HTTP server ready; / operator, /dev engineering; Ethernet primary, Wi-Fi optional STA");
    return ESP_OK;
}

}  // namespace sp01
