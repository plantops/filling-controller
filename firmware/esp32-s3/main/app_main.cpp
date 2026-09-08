#include "sp01/board_io.hpp"
#include "sp01/controller.hpp"
#include "sp01/tlb485.hpp"
#include "sp01/web_hmi.hpp"

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace {

constexpr char kTag[] = "sp01";

sp01::Controller* g_controller = nullptr;
sp01::BoardIo* g_io = nullptr;
sp01::Tlb485* g_tlb = nullptr;
portMUX_TYPE g_status_mux = portMUX_INITIALIZER_UNLOCKED;
sp01::InputImage g_inputs{};
sp01::ControllerSnapshot g_controller_snapshot{};
sp01::ControllerConfig g_controller_config{};

sp01::ControllerConfig make_controller_config() noexcept {
    sp01::ControllerConfig c{};
    c.target_kg = static_cast<float>(CONFIG_SP01_TARGET_G) / 1000.0F;
    c.coarse_to_fine_kg = static_cast<float>(CONFIG_SP01_COARSE_TO_FINE_G) / 1000.0F;
    c.cutoff_margin_kg = static_cast<float>(CONFIG_SP01_CUTOFF_MARGIN_G) / 1000.0F;
    c.weight_stale_us = static_cast<std::uint64_t>(CONFIG_SP01_WEIGHT_STALE_MS) * 1000ULL;
    c.bag_acquire_timeout_us = static_cast<std::uint64_t>(CONFIG_SP01_BAG_ACQUIRE_TIMEOUT_MS) * 1000ULL;
    c.coarse_timeout_us = static_cast<std::uint64_t>(CONFIG_SP01_COARSE_TIMEOUT_MS) * 1000ULL;
    c.fine_timeout_us = static_cast<std::uint64_t>(CONFIG_SP01_FINE_TIMEOUT_MS) * 1000ULL;
    c.settle_min_us = static_cast<std::uint64_t>(CONFIG_SP01_SETTLE_MIN_MS) * 1000ULL;
    c.wait_push_timeout_us = static_cast<std::uint64_t>(CONFIG_SP01_WAIT_PUSH_TIMEOUT_MS) * 1000ULL;
    c.push_duration_us = static_cast<std::uint64_t>(CONFIG_SP01_PUSH_DURATION_MS) * 1000ULL;
    return c;
}

bool weight_fresh_now(const sp01::WeightSnapshot& weight) noexcept {
    if (weight.quality != sp01::WeightQuality::Good) return false;
    const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
    return weight.sample_time_us <= now && now - weight.sample_time_us <= g_controller_config.weight_stale_us;
}

bool fill_hmi_snapshot(sp01::HmiSnapshot& out) noexcept {
    if (!g_controller || !g_tlb) return false;
    portENTER_CRITICAL(&g_status_mux);
    out.inputs = g_inputs;
    out.controller = g_controller_snapshot;
    portEXIT_CRITICAL(&g_status_mux);
    out.weight = g_tlb->snapshot();
    out.tlb = g_tlb->diagnostics();
    out.service_ready = sp01::input(out.inputs, sp01::Di::Spare) &&
                        !sp01::machine_permissive(out.inputs) &&
                        out.controller.state == sp01::State::WaitPermissive &&
                        sp01::all_outputs_off(out.controller.outputs) &&
                        out.weight.stable && weight_fresh_now(out.weight);
    return true;
}

bool service_ready() noexcept {
    sp01::HmiSnapshot s{};
    return fill_hmi_snapshot(s) && s.service_ready;
}

esp_err_t hmi_cal_zero() noexcept {
    if (!g_tlb || !service_ready()) return ESP_ERR_NOT_ALLOWED;
    return g_tlb->calibration_zero();
}

esp_err_t hmi_cal_span(float kg) noexcept {
    if (!g_tlb || !service_ready()) return ESP_ERR_NOT_ALLOWED;
    return g_tlb->calibration_span(kg);
}

void weighing_task(void*) {
#if CONFIG_SP01_TLB_ENABLE
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
        const esp_err_t err = g_tlb->poll_once(now);
        if (err != ESP_OK) ESP_LOGD(kTag, "TLB poll: %s", esp_err_to_name(err));
        vTaskDelayUntil(&last, pdMS_TO_TICKS(CONFIG_SP01_TLB_POLL_MS));
    }
#else
    vTaskDelete(nullptr);
#endif
}

void control_task(void*) {
    (void)esp_task_wdt_add(nullptr);
    TickType_t last = xTaskGetTickCount();
    auto previous_state = g_controller->snapshot().state;

    for (;;) {
        const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
        sp01::InputImage inputs{};
        esp_err_t io_err = g_io->read_inputs(inputs);
        sp01::ControllerSnapshot snapshot{};

        if (io_err != ESP_OK) {
            g_controller->force_fault(sp01::Fault::IoFault, now);
            snapshot = g_controller->snapshot();
            (void)g_io->force_safe();
        } else {
            const auto weight = g_tlb->snapshot();
            snapshot = g_controller->tick(now, inputs, weight);
            io_err = g_io->commit_outputs(snapshot.outputs);
            if (io_err != ESP_OK) {
                (void)g_io->force_safe();
                g_controller->force_fault(sp01::Fault::IoFault, now);
                snapshot = g_controller->snapshot();
            }
        }

        portENTER_CRITICAL(&g_status_mux);
        g_inputs = inputs;
        g_controller_snapshot = snapshot;
        portEXIT_CRITICAL(&g_status_mux);

        if (snapshot.state != previous_state) {
            ESP_LOGI(kTag, "%s -> %s fault=%s",
                     sp01::state_name(previous_state), sp01::state_name(snapshot.state),
                     sp01::fault_name(snapshot.fault));
            previous_state = snapshot.state;
        }

        (void)esp_task_wdt_reset();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(CONFIG_SP01_CONTROL_PERIOD_MS));
    }
}

}  // namespace

extern "C" void app_main(void) {
    static sp01::BoardIo io;
    static sp01::Tlb485 tlb;
    g_controller_config = make_controller_config();
    static sp01::Controller controller(g_controller_config);

    g_io = &io;
    g_tlb = &tlb;
    g_controller = &controller;

    ESP_LOGI(kTag, "SP01 v0.1 ESP-IDF/C++ controller");
    ESP_ERROR_CHECK(io.init(static_cast<std::uint8_t>(CONFIG_SP01_DI_INVERT_MASK),
                            static_cast<std::uint8_t>(CONFIG_SP01_DO_INVERT_MASK)));
    ESP_ERROR_CHECK(io.force_safe());

#if CONFIG_SP01_TLB_ENABLE
    sp01::Tlb485Config tlb_cfg{};
    tlb_cfg.baud = CONFIG_SP01_TLB_BAUD;
    tlb_cfg.slave = CONFIG_SP01_TLB_SLAVE;
    tlb_cfg.calibration_writes = CONFIG_SP01_TLB_CALIBRATION_WRITES;
    ESP_ERROR_CHECK(tlb.init(tlb_cfg));
#endif

    portENTER_CRITICAL(&g_status_mux);
    g_controller_snapshot = controller.snapshot();
    portEXIT_CRITICAL(&g_status_mux);

    xTaskCreatePinnedToCore(control_task, "sp01_control", 4096, nullptr, 15, nullptr, 1);
#if CONFIG_SP01_TLB_ENABLE
    xTaskCreatePinnedToCore(weighing_task, "sp01_tlb", 4096, nullptr, 10, nullptr, 0);
#endif

    sp01::WebHmiConfig web{};
    web.ssid = CONFIG_SP01_WIFI_SSID;
    web.password = CONFIG_SP01_WIFI_PASSWORD;
    web.service_token = CONFIG_SP01_SERVICE_TOKEN;
    const esp_err_t web_err = sp01::web_hmi_start(web, fill_hmi_snapshot, hmi_cal_zero, hmi_cal_span);
    if (web_err != ESP_OK) ESP_LOGW(kTag, "HMI disabled: %s", esp_err_to_name(web_err));
}
