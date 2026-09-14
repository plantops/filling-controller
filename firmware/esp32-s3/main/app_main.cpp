#include "sp01/board_io.hpp"
#include "sp01/controller.hpp"
#include "sp01/controller_explain.hpp"
#include "sp01/position_decoder.hpp"
#include "sp01/time_shift.hpp"
#include "sp01/trace.hpp"
#include "sp01/tlb485.hpp"
#include "sp01/web_hmi.hpp"

#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <cinttypes>
#include <cstdio>

namespace {

constexpr char kTag[] = "sp01";
constexpr char kFieldApSsid[] = "SP01-HMI";
constexpr char kFieldApPassword[] = "sp01filling";

sp01::Controller* g_controller = nullptr;
sp01::BoardIo* g_io = nullptr;
sp01::Tlb485* g_tlb = nullptr;
portMUX_TYPE g_status_mux = portMUX_INITIALIZER_UNLOCKED;
sp01::InputImage g_inputs{};
sp01::WeightSnapshot g_weight_snapshot{};
sp01::ControllerSnapshot g_controller_snapshot{};
sp01::OutputImage g_commanded_outputs{};

sp01::PositionDecoder g_position{};
sp01::PositionSnapshot g_last_position{};
sp01::TimeKeeper g_clock{};
sp01::ShiftTracker g_shifts{};

// Edges are recorded here, at tick rate, so a 20 ms output pulse is still
// visible to the HMI minutes later. The browser used to diff two polls 300 ms
// apart, which cannot see a pulse narrower than the poll.
sp01::TraceBuffer g_trace{};
sp01::TraceRecorder g_recorder{};

float g_target_pending_kg = 0.0F;
bool g_had_last_bag = false;
float g_last_bag_kg = 0.0F;
char g_last_bag_time[12] = "";
std::uint32_t g_last_counted_cycle = 0;

bool spout_clear(sp01::State state) noexcept {
    switch (state) {
        case sp01::State::WaitPermissive:
        case sp01::State::WaitFillPosition:
        case sp01::State::Complete:
        case sp01::State::Fault:
            return true;
        default:
            return false;
    }
}

sp01::HmiResult on_target_request(float kg) {
    portENTER_CRITICAL(&g_status_mux);
    g_target_pending_kg = kg;
    portEXIT_CRITICAL(&g_status_mux);
    ESP_LOGI(kTag, "target %.1f kg queued until the spout is clear", static_cast<double>(kg));
    return sp01::HmiResult::Ok;
}

void on_browser_time(std::uint64_t unix_ms, std::int16_t tz_offset_min) {
    const bool first = !g_clock.synced();
    g_clock.sync_from_browser(static_cast<std::uint64_t>(esp_timer_get_time()), unix_ms, tz_offset_min);
    if (first) ESP_LOGI(kTag, "clock synced from browser");
}

sp01::ControllerConfig g_controller_config{};
std::uint8_t g_bench_do_channel = 0;
std::uint64_t g_bench_do_until_us = 0;

#if CONFIG_SP01_DUMMY_WEIGHT_ENABLE
float g_dummy_weight_kg = 0.0F;
std::uint32_t g_dummy_weight_sequence = 0;
#endif

sp01::ControllerConfig make_controller_config() noexcept {
    sp01::ControllerConfig c{};
    c.target_kg = static_cast<float>(CONFIG_SP01_TARGET_G) / 1000.0F;
    c.coarse_to_fine_kg = static_cast<float>(CONFIG_SP01_COARSE_TO_FINE_G) / 1000.0F;
    c.cutoff_margin_kg = static_cast<float>(CONFIG_SP01_CUTOFF_MARGIN_G) / 1000.0F;
    c.broken_bag_loss_trip_kg = static_cast<float>(CONFIG_SP01_BROKEN_BAG_LOSS_TRIP_G) / 1000.0F;
    c.broken_bag_persist_us = static_cast<std::uint64_t>(CONFIG_SP01_BROKEN_BAG_PERSIST_MS) * 1000ULL;
    c.reject_wait_timeout_us = static_cast<std::uint64_t>(CONFIG_SP01_REJECT_WAIT_TIMEOUT_MS) * 1000ULL;
    c.weight_stale_us = static_cast<std::uint64_t>(CONFIG_SP01_WEIGHT_STALE_MS) * 1000ULL;
    c.bag_acquire_timeout_us = static_cast<std::uint64_t>(CONFIG_SP01_BAG_ACQUIRE_TIMEOUT_MS) * 1000ULL;
    c.coarse_timeout_us = static_cast<std::uint64_t>(CONFIG_SP01_COARSE_TIMEOUT_MS) * 1000ULL;
    c.fine_timeout_us = static_cast<std::uint64_t>(CONFIG_SP01_FINE_TIMEOUT_MS) * 1000ULL;
    c.settle_min_us = static_cast<std::uint64_t>(CONFIG_SP01_SETTLE_MIN_MS) * 1000ULL;
    c.wait_discharge_timeout_us = static_cast<std::uint64_t>(CONFIG_SP01_WAIT_DISCHARGE_TIMEOUT_MS) * 1000ULL;
    c.push_duration_us = static_cast<std::uint64_t>(CONFIG_SP01_PUSH_DURATION_MS) * 1000ULL;
    c.discharge_angle_deg = static_cast<float>(CONFIG_SP01_DISCHARGE_ANGLE_DEG);
    c.reject_angle_deg = static_cast<float>(CONFIG_SP01_REJECT_ANGLE_DEG);
    c.discharge_lead_deg = static_cast<float>(CONFIG_SP01_DISCHARGE_LEAD_DEG);
    return c;
}

bool valid_broken_bag_config(const sp01::ControllerConfig& c) noexcept {
    const bool has_loss = c.broken_bag_loss_trip_kg > 0.0F;
    const bool has_persist = c.broken_bag_persist_us > 0;
    const bool has_timeout = c.reject_wait_timeout_us > 0;
    const bool any = has_loss || has_persist || has_timeout;
    const bool all = has_loss && has_persist && has_timeout;
    return !any || all;
}

void log_build_and_config_identity() noexcept {
    const esp_app_desc_t* app = esp_app_get_description();
    if (app) {
        ESP_LOGI(kTag,
                 "build project=%s version=%s idf=%s elf_sha256=%02x%02x%02x%02x%02x%02x%02x%02x reset_reason=%d",
                 app->project_name, app->version, app->idf_ver,
                 static_cast<unsigned>(app->app_elf_sha256[0]), static_cast<unsigned>(app->app_elf_sha256[1]),
                 static_cast<unsigned>(app->app_elf_sha256[2]), static_cast<unsigned>(app->app_elf_sha256[3]),
                 static_cast<unsigned>(app->app_elf_sha256[4]), static_cast<unsigned>(app->app_elf_sha256[5]),
                 static_cast<unsigned>(app->app_elf_sha256[6]), static_cast<unsigned>(app->app_elf_sha256[7]),
                 static_cast<int>(esp_reset_reason()));
    }
    ESP_LOGI(kTag,
             "config control_ms=%d target_g=%d coarse_to_fine_g=%d cutoff_margin_g=%d weight_stale_ms=%d "
             "broken_loss_g=%d broken_persist_ms=%d reject_timeout_ms=%d discharge_deg=%d reject_deg=%d lead_deg=%d "
             "di_invert=0x%02x do_invert=0x%02x tlb_enable=%d dummy_weight=%d tlb_baud=%d tlb_slave=%d tlb_poll_ms=%d",
             CONFIG_SP01_CONTROL_PERIOD_MS, CONFIG_SP01_TARGET_G, CONFIG_SP01_COARSE_TO_FINE_G,
             CONFIG_SP01_CUTOFF_MARGIN_G, CONFIG_SP01_WEIGHT_STALE_MS,
             CONFIG_SP01_BROKEN_BAG_LOSS_TRIP_G, CONFIG_SP01_BROKEN_BAG_PERSIST_MS,
             CONFIG_SP01_REJECT_WAIT_TIMEOUT_MS, CONFIG_SP01_DISCHARGE_ANGLE_DEG,
             CONFIG_SP01_REJECT_ANGLE_DEG, CONFIG_SP01_DISCHARGE_LEAD_DEG,
             CONFIG_SP01_DI_INVERT_MASK, CONFIG_SP01_DO_INVERT_MASK,
#if CONFIG_SP01_TLB_ENABLE
             1,
#else
             0,
#endif
#if CONFIG_SP01_DUMMY_WEIGHT_ENABLE
             1,
#else
             0,
#endif
             CONFIG_SP01_TLB_BAUD, CONFIG_SP01_TLB_SLAVE, CONFIG_SP01_TLB_POLL_MS);
}

#if CONFIG_SP01_DUMMY_WEIGHT_ENABLE
sp01::WeightSnapshot dummy_weight(std::uint64_t now_us, sp01::State state) noexcept {
    constexpr float kCoarseStepKg = 0.20F;
    constexpr float kFineStepKg = 0.05F;
    bool stable = true;
    switch (state) {
        case sp01::State::WaitPermissive:
        case sp01::State::WaitFillPosition:
        case sp01::State::BagAcquire:
        case sp01::State::BagVerify:
        case sp01::State::TareReady:
            g_dummy_weight_kg = 0.0F;
            break;
        case sp01::State::CoarseFill:
            stable = false;
            g_dummy_weight_kg += kCoarseStepKg;
            if (g_dummy_weight_kg > g_controller_config.coarse_to_fine_kg) g_dummy_weight_kg = g_controller_config.coarse_to_fine_kg;
            break;
        case sp01::State::FineFill:
            stable = false;
            g_dummy_weight_kg += kFineStepKg;
            if (g_dummy_weight_kg > g_controller_config.target_kg) g_dummy_weight_kg = g_controller_config.target_kg;
            break;
        case sp01::State::Cutoff:
            stable = false;
            if (g_dummy_weight_kg < g_controller_config.target_kg) g_dummy_weight_kg = g_controller_config.target_kg;
            break;
        case sp01::State::Settle:
        case sp01::State::RejectWait:
        case sp01::State::WaitDischarge:
        case sp01::State::Push:
        case sp01::State::Complete:
        case sp01::State::Fault:
            break;
    }
    sp01::WeightSnapshot w{};
    w.net_kg = g_dummy_weight_kg;
    w.sample_time_us = now_us;
    w.sequence = ++g_dummy_weight_sequence;
    w.quality = sp01::WeightQuality::Good;
    w.stable = stable;
    return w;
}
#endif

sp01::WeightSnapshot current_weight(std::uint64_t now_us) noexcept {
#if CONFIG_SP01_DUMMY_WEIGHT_ENABLE
    return dummy_weight(now_us, g_controller->snapshot().state);
#elif CONFIG_SP01_TLB_ENABLE
    return g_tlb->snapshot();
#else
    (void)now_us;
    return {};
#endif
}

#if CONFIG_SP01_TLB_ENABLE
void weighing_task(void*) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
        const esp_err_t err = g_tlb->poll_once(now);
        if (err != ESP_OK) ESP_LOGD(kTag, "TLB poll: %s", esp_err_to_name(err));
        vTaskDelayUntil(&last, pdMS_TO_TICKS(CONFIG_SP01_TLB_POLL_MS));
    }
}
#endif

void control_task(void*) {
    (void)esp_task_wdt_add(nullptr);
    TickType_t last = xTaskGetTickCount();
    auto previous_state = g_controller->snapshot().state;

    for (;;) {
        const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
        sp01::InputImage inputs{};
        esp_err_t io_err = g_io->read_inputs(inputs);
        sp01::ControllerSnapshot snapshot{};
        sp01::OutputImage commanded_outputs{};
        sp01::WeightSnapshot weight{};

        if (io_err != ESP_OK) {
            g_controller->force_fault(sp01::Fault::IoFault, now);
            snapshot = g_controller->snapshot();
            (void)g_io->force_safe();
            commanded_outputs = g_io->last_commanded_outputs();
        } else {
            inputs.mode = sp01::input(inputs, sp01::Di::MachineMotorRunning) ? sp01::OperationMode::Auto : sp01::OperationMode::Manual;

            g_position.update(now, sp01::input(inputs, sp01::Di::PositionIndex), sp01::input(inputs, sp01::Di::PositionMark));
            sp01::PositionSnapshot position{};
            position.valid = g_position.usable();
            position.angle_deg = g_position.status().angle_deg;
            position.revolution_us = g_position.status().revolution_us;
            g_last_position = position;

            float queued = 0.0F;
            portENTER_CRITICAL(&g_status_mux);
            queued = g_target_pending_kg;
            portEXIT_CRITICAL(&g_status_mux);
            if (queued > 0.0F && spout_clear(g_controller->snapshot().state)) {
                g_controller->set_target_kg(queued);
                portENTER_CRITICAL(&g_status_mux);
                g_target_pending_kg = 0.0F;
                portEXIT_CRITICAL(&g_status_mux);
                ESP_LOGI(kTag, "target now %.1f kg", static_cast<double>(queued));
            }

            weight = current_weight(now);
            snapshot = g_controller->tick(now, inputs, weight, position);

            sp01::OutputImage physical_outputs = snapshot.outputs;
#if CONFIG_SP01_DUMMY_WEIGHT_ENABLE || CONFIG_SP01_BENCH_DO_TEST_ENABLE
            physical_outputs = sp01::safe_output_image();
#endif
#if CONFIG_SP01_BENCH_DO_TEST_ENABLE
            std::uint8_t bench_channel = 0;
            std::uint64_t bench_until = 0;
            portENTER_CRITICAL(&g_status_mux);
            bench_channel = g_bench_do_channel;
            bench_until = g_bench_do_until_us;
            if (bench_channel != 0 && now >= bench_until) {
                g_bench_do_channel = 0;
                g_bench_do_until_us = 0;
                bench_channel = 0;
            }
            portEXIT_CRITICAL(&g_status_mux);
            if (bench_channel >= 1 && bench_channel <= 8) physical_outputs.channels[bench_channel - 1] = true;
#endif
            io_err = g_io->commit_outputs(physical_outputs);
            if (io_err != ESP_OK) {
                (void)g_io->force_safe();
                g_controller->force_fault(sp01::Fault::IoFault, now);
                snapshot = g_controller->snapshot();
            }
            commanded_outputs = g_io->last_commanded_outputs();
        }

        portENTER_CRITICAL(&g_status_mux);
        g_inputs = inputs;
        g_weight_snapshot = weight;
        g_controller_snapshot = snapshot;
        g_commanded_outputs = commanded_outputs;
        portEXIT_CRITICAL(&g_status_mux);

        {
            portENTER_CRITICAL(&g_status_mux);
            g_recorder.observe(now, inputs, snapshot, commanded_outputs,
                               g_last_position.valid, g_trace);
            g_recorder.observe_target(now, g_controller->config().target_kg,
                                      g_trace);
            portEXIT_CRITICAL(&g_status_mux);
        }

        g_shifts.update(g_clock, now);
        if (snapshot.state == sp01::State::Complete && snapshot.cycle_id != g_last_counted_cycle) {
            g_last_counted_cycle = snapshot.cycle_id;
            g_shifts.record_bag(weight.net_kg, snapshot.disposition == sp01::BagDisposition::Reject);
            g_had_last_bag = true;
            g_last_bag_kg = weight.net_kg;
            if (g_clock.synced()) {
                const sp01::CivilTime t = g_clock.civil(now);
                std::snprintf(g_last_bag_time, sizeof(g_last_bag_time), "%02u:%02u:%02u", t.hour, t.minute, t.second);
            }
        }

        {
            sp01::HmiPublish pub{};
            pub.snapshot = snapshot;
            pub.explain = sp01::explain_controller(snapshot, g_controller->config(), inputs, weight, g_last_position, now);
            pub.inputs = inputs;
            pub.weight = weight;
            pub.commanded_outputs = commanded_outputs;
            pub.position = g_last_position;
#if CONFIG_SP01_DUMMY_WEIGHT_ENABLE || CONFIG_SP01_BENCH_DO_TEST_ENABLE
            pub.shadow_mode = true;
#else
            pub.shadow_mode = false;
#endif
            pub.weight_kg = weight.net_kg;
            pub.target_kg = g_controller->config().target_kg;
            pub.target_pending_kg = g_target_pending_kg;
            pub.has_last_bag = g_had_last_bag;
            pub.last_bag_kg = g_last_bag_kg;
            std::snprintf(pub.last_bag_time, sizeof(pub.last_bag_time), "%s", g_last_bag_time);
            pub.time_synced = g_clock.synced();
            pub.civil = g_clock.civil(now);
            pub.shift_no = g_clock.shift_no(now);
            pub.shift = g_shifts.shift();
            pub.day = g_shifts.day();
            pub.unattributed = g_shifts.unattributed();
            sp01::hmi_publish(pub);
        }

        if (snapshot.state != previous_state) {
            ESP_LOGI(kTag, "%s -> %s fault=%s", sp01::state_name(previous_state), sp01::state_name(snapshot.state), sp01::fault_name(snapshot.fault));
            previous_state = snapshot.state;
        }
        (void)esp_task_wdt_reset();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(CONFIG_SP01_CONTROL_PERIOD_MS));
    }
}

void start_field_hotspot_if_needed() noexcept {
    if (CONFIG_SP01_WIFI_SSID[0] != '\0') {
        ESP_LOGI(kTag, "Field hotspot skipped: configured Wi-Fi STA is enabled");
        return;
    }
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) {
        ESP_LOGW(kTag, "Field hotspot unavailable: AP netif create failed");
        return;
    }
    (void)esp_netif_set_hostname(ap_netif, "sp01-ap");
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&init);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Field hotspot unavailable: Wi-Fi init failed: %s", esp_err_to_name(err));
        return;
    }
    wifi_config_t ap{};
    std::snprintf(reinterpret_cast<char*>(ap.ap.ssid), sizeof(ap.ap.ssid), "%s", kFieldApSsid);
    std::snprintf(reinterpret_cast<char*>(ap.ap.password), sizeof(ap.ap.password), "%s", kFieldApPassword);
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Field hotspot unavailable: %s", esp_err_to_name(err));
        return;
    }
    esp_netif_ip_info_t ip{};
    if (esp_netif_get_ip_info(ap_netif, &ip) == ESP_OK) {
        ESP_LOGI(kTag, "FIELD HOTSPOT READY: SSID=%s HMI=http://" IPSTR, kFieldApSsid, IP2STR(&ip.ip));
    } else {
        ESP_LOGI(kTag, "FIELD HOTSPOT READY: SSID=%s HMI=http://192.168.4.1", kFieldApSsid);
    }
}

}  // namespace

namespace sp01 {
// The HMI reads the trace under the same lock the control loop writes it with.
std::size_t hmi_read_trace(std::uint32_t since_seq, TraceEvent* out,
                           std::size_t cap, std::uint32_t* lost,
                           std::uint32_t* last_seq) noexcept {
    portENTER_CRITICAL(&g_status_mux);
    const std::size_t n = g_trace.since(since_seq, out, cap, lost);
    if (last_seq != nullptr) *last_seq = g_trace.last_seq();
    portEXIT_CRITICAL(&g_status_mux);
    return n;
}
}  // namespace sp01

extern "C" void app_main(void) {
    static sp01::BoardIo io;
    static sp01::Tlb485 tlb;
    g_controller_config = make_controller_config();
    if (!valid_broken_bag_config(g_controller_config)) {
        ESP_LOGE(kTag, "invalid broken-bag config: loss trip, persistence and reject timeout must be all zero or all non-zero");
        ESP_ERROR_CHECK(ESP_ERR_INVALID_ARG);
    }
    static sp01::Controller controller(g_controller_config);
    g_io = &io;
    g_tlb = &tlb;
    g_controller = &controller;

    ESP_LOGI(kTag, "SP01 v0.1 ESP-IDF/C++ controller");
    log_build_and_config_identity();
    if (g_controller_config.broken_bag_loss_trip_kg <= 0.0F) ESP_LOGW(kTag, "broken-bag detector disabled pending G4/G8 measured commissioning values");
#if CONFIG_SP01_DUMMY_WEIGHT_ENABLE
    ESP_LOGW(kTag, "FIELD PROTOTYPE SHADOW: dummy weight active; normal process DO suppressed");
#endif
#if CONFIG_SP01_BENCH_DO_TEST_ENABLE
    ESP_LOGW(kTag, "G2 BENCH MODE: normal process outputs suppressed; HMI one-hot DO pulse only");
#endif
    ESP_ERROR_CHECK(io.init(static_cast<std::uint8_t>(CONFIG_SP01_DI_INVERT_MASK), static_cast<std::uint8_t>(CONFIG_SP01_DO_INVERT_MASK)));
    ESP_ERROR_CHECK(io.force_safe());

#if CONFIG_SP01_TLB_ENABLE
    sp01::Tlb485Config tlb_cfg{};
    tlb_cfg.baud = CONFIG_SP01_TLB_BAUD;
    tlb_cfg.slave = CONFIG_SP01_TLB_SLAVE;
#if CONFIG_SP01_TLB_CALIBRATION_WRITES
    tlb_cfg.calibration_writes = true;
#else
    tlb_cfg.calibration_writes = false;
#endif
    ESP_ERROR_CHECK(tlb.init(tlb_cfg));
#endif

    portENTER_CRITICAL(&g_status_mux);
    g_weight_snapshot = current_weight(static_cast<std::uint64_t>(esp_timer_get_time()));
    g_controller_snapshot = controller.snapshot();
    g_commanded_outputs = io.last_commanded_outputs();
    portEXIT_CRITICAL(&g_status_mux);

    xTaskCreatePinnedToCore(control_task, "sp01_control", 4096, nullptr, 15, nullptr, 1);
#if CONFIG_SP01_TLB_ENABLE
    xTaskCreatePinnedToCore(weighing_task, "sp01_tlb", 4096, nullptr, 10, nullptr, 0);
#endif

    const esp_err_t gpio_isr_err = gpio_install_isr_service(0);
    if (gpio_isr_err != ESP_OK && gpio_isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "GPIO ISR service init failed: %s", esp_err_to_name(gpio_isr_err));
    } else {
        ESP_LOGI(kTag, "GPIO ISR service ready for W5500 IRQ");
    }

    sp01::HmiIdentity identity{};
    std::snprintf(identity.machine, sizeof(identity.machine), "%s", CONFIG_SP01_MACHINE_ID);
    std::snprintf(identity.spout, sizeof(identity.spout), "%s", CONFIG_SP01_SPOUT_ID);
    const esp_app_desc_t* app_desc = esp_app_get_description();
    if (app_desc != nullptr) std::snprintf(identity.firmware, sizeof(identity.firmware), "%s", app_desc->version);

    sp01::HmiPins pins{};
    std::snprintf(pins.operator_pin, sizeof(pins.operator_pin), "%s", CONFIG_SP01_OPERATOR_PIN);
    std::snprintf(pins.supervisor_pin, sizeof(pins.supervisor_pin), "%s", CONFIG_SP01_SUPERVISOR_PIN);

    sp01::HmiCallbacks callbacks{};
    callbacks.request_target = &on_target_request;
    callbacks.set_time = &on_browser_time;

    const esp_err_t web_err = sp01::hmi_start(identity, pins, callbacks);
    if (web_err != ESP_OK) {
        ESP_LOGW(kTag, "HMI disabled: %s", esp_err_to_name(web_err));
    } else {
        start_field_hotspot_if_needed();
    }
}
