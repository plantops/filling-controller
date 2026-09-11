#include "sp01/board_io.hpp"
#include "sp01/controller.hpp"
#include "sp01/tlb485.hpp"
#include "sp01/web_hmi.hpp"

#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <cinttypes>

namespace {

constexpr char kTag[] = "sp01";

sp01::Controller* g_controller = nullptr;
sp01::BoardIo* g_io = nullptr;
sp01::Tlb485* g_tlb = nullptr;
portMUX_TYPE g_status_mux = portMUX_INITIALIZER_UNLOCKED;
sp01::InputImage g_inputs{};
sp01::ControllerSnapshot g_controller_snapshot{};
sp01::OutputImage g_commanded_outputs{};
sp01::ControllerConfig g_controller_config{};
std::uint8_t g_bench_do_channel = 0;
std::uint64_t g_bench_do_until_us = 0;

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
    c.discharge_countdown_counts = static_cast<std::uint32_t>(CONFIG_SP01_DISCHARGE_COUNTDOWN_COUNTS);
    c.discharge_lead_counts = static_cast<std::uint32_t>(CONFIG_SP01_DISCHARGE_LEAD_COUNTS);
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
                 static_cast<unsigned>(app->app_elf_sha256[0]),
                 static_cast<unsigned>(app->app_elf_sha256[1]),
                 static_cast<unsigned>(app->app_elf_sha256[2]),
                 static_cast<unsigned>(app->app_elf_sha256[3]),
                 static_cast<unsigned>(app->app_elf_sha256[4]),
                 static_cast<unsigned>(app->app_elf_sha256[5]),
                 static_cast<unsigned>(app->app_elf_sha256[6]),
                 static_cast<unsigned>(app->app_elf_sha256[7]),
                 static_cast<int>(esp_reset_reason()));
    }
    ESP_LOGI(kTag,
             "config control_ms=%d target_g=%d coarse_to_fine_g=%d cutoff_margin_g=%d weight_stale_ms=%d "
             "broken_loss_g=%d broken_persist_ms=%d reject_timeout_ms=%d discharge_counts=%d discharge_lead=%d "
             "di_invert=0x%02x do_invert=0x%02x tlb_enable=%d tlb_baud=%d tlb_slave=%d tlb_poll_ms=%d",
             CONFIG_SP01_CONTROL_PERIOD_MS, CONFIG_SP01_TARGET_G, CONFIG_SP01_COARSE_TO_FINE_G,
             CONFIG_SP01_CUTOFF_MARGIN_G, CONFIG_SP01_WEIGHT_STALE_MS,
             CONFIG_SP01_BROKEN_BAG_LOSS_TRIP_G, CONFIG_SP01_BROKEN_BAG_PERSIST_MS,
             CONFIG_SP01_REJECT_WAIT_TIMEOUT_MS, CONFIG_SP01_DISCHARGE_COUNTDOWN_COUNTS,
             CONFIG_SP01_DISCHARGE_LEAD_COUNTS, CONFIG_SP01_DI_INVERT_MASK, CONFIG_SP01_DO_INVERT_MASK,
#if CONFIG_SP01_TLB_ENABLE
             1,
#else
             0,
#endif
             CONFIG_SP01_TLB_BAUD, CONFIG_SP01_TLB_SLAVE, CONFIG_SP01_TLB_POLL_MS);
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
    out.commanded_outputs = g_commanded_outputs;
    portEXIT_CRITICAL(&g_status_mux);
    out.weight = g_tlb->snapshot();
    out.tlb = g_tlb->diagnostics();

    const bool machine_stopped = !sp01::input(out.inputs, sp01::Di::MachineMotorRunning);
    const bool fill_switch_off = !sp01::input(out.inputs, sp01::Di::ProcessInitiative);
    out.service_ready = machine_stopped && fill_switch_off &&
                        out.controller.state == sp01::State::WaitPermissive &&
                        sp01::all_outputs_off(out.controller.outputs) &&
                        sp01::all_outputs_off(out.commanded_outputs) &&
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

esp_err_t hmi_bench_do_pulse(std::uint8_t channel, std::uint32_t pulse_ms) noexcept {
#if CONFIG_SP01_BENCH_DO_TEST_ENABLE
    if (channel < 1 || channel > 8 || pulse_ms == 0 || pulse_ms > 1000) return ESP_ERR_INVALID_ARG;
    const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
    portENTER_CRITICAL(&g_status_mux);
    g_bench_do_channel = channel;
    g_bench_do_until_us = now + static_cast<std::uint64_t>(pulse_ms) * 1000ULL;
    portEXIT_CRITICAL(&g_status_mux);
    ESP_LOGW(kTag, "G2 BENCH pulse DO%u for %" PRIu32 " ms", static_cast<unsigned>(channel), pulse_ms);
    return ESP_OK;
#else
    (void)channel;
    (void)pulse_ms;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t hmi_bench_do_off() noexcept {
#if CONFIG_SP01_BENCH_DO_TEST_ENABLE
    portENTER_CRITICAL(&g_status_mux);
    g_bench_do_channel = 0;
    g_bench_do_until_us = 0;
    portEXIT_CRITICAL(&g_status_mux);
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
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

        if (io_err != ESP_OK) {
            g_controller->force_fault(sp01::Fault::IoFault, now);
            snapshot = g_controller->snapshot();
            (void)g_io->force_safe();
            commanded_outputs = g_io->last_commanded_outputs();
        } else {
            inputs.mode = sp01::input(inputs, sp01::Di::MachineMotorRunning)
                              ? sp01::OperationMode::Auto
                              : sp01::OperationMode::Manual;

            const auto weight = g_tlb->snapshot();
            snapshot = g_controller->tick(now, inputs, weight);

            sp01::OutputImage physical_outputs = snapshot.outputs;
#if CONFIG_SP01_BENCH_DO_TEST_ENABLE
            // G2 bench artifact is deliberately non-operational: normal process
            // outputs are suppressed. Only one explicit HMI-requested DO may pulse,
            // and it automatically returns to all-off after <= 1 second.
            physical_outputs = sp01::safe_output_image();
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
            if (bench_channel >= 1 && bench_channel <= 8) {
                physical_outputs.channels[bench_channel - 1] = true;
            }
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
        g_controller_snapshot = snapshot;
        g_commanded_outputs = commanded_outputs;
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
    if (!valid_broken_bag_config(g_controller_config)) {
        ESP_LOGE(kTag,
                 "invalid broken-bag config: loss trip, persistence and reject timeout must be all zero or all non-zero");
        ESP_ERROR_CHECK(ESP_ERR_INVALID_ARG);
    }
    static sp01::Controller controller(g_controller_config);

    g_io = &io;
    g_tlb = &tlb;
    g_controller = &controller;

    ESP_LOGI(kTag, "SP01 v0.1 ESP-IDF/C++ controller");
    log_build_and_config_identity();
    if (g_controller_config.broken_bag_loss_trip_kg <= 0.0F) {
        ESP_LOGW(kTag, "broken-bag detector disabled pending G4/G8 measured commissioning values");
    }
#if CONFIG_SP01_BENCH_DO_TEST_ENABLE
    ESP_LOGW(kTag, "G2 BENCH BUILD: normal process outputs suppressed; HMI one-hot DO pulse only");
#endif
    ESP_ERROR_CHECK(io.init(static_cast<std::uint8_t>(CONFIG_SP01_DI_INVERT_MASK),
                            static_cast<std::uint8_t>(CONFIG_SP01_DO_INVERT_MASK)));
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
    g_controller_snapshot = controller.snapshot();
    g_commanded_outputs = io.last_commanded_outputs();
    portEXIT_CRITICAL(&g_status_mux);

    xTaskCreatePinnedToCore(control_task, "sp01_control", 4096, nullptr, 15, nullptr, 1);
#if CONFIG_SP01_TLB_ENABLE
    xTaskCreatePinnedToCore(weighing_task, "sp01_tlb", 4096, nullptr, 10, nullptr, 0);
#endif

    // ESP-IDF's W5500 interrupt mode requires the GPIO ISR service to exist
    // before the Ethernet driver registers the IRQ handler. The previous G2
    // build omitted this, so W5500 could fail before DHCP ever started.
    const esp_err_t gpio_isr_err = gpio_install_isr_service(0);
    if (gpio_isr_err != ESP_OK && gpio_isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "GPIO ISR service init failed: %s", esp_err_to_name(gpio_isr_err));
    } else {
        ESP_LOGI(kTag, "GPIO ISR service ready for W5500 IRQ");
    }

    sp01::WebHmiConfig web{};
    web.ssid = CONFIG_SP01_WIFI_SSID;
    web.password = CONFIG_SP01_WIFI_PASSWORD;
    web.service_token = CONFIG_SP01_SERVICE_TOKEN;
#if CONFIG_SP01_BENCH_DO_TEST_ENABLE
    web.bench_do_enabled = true;
#endif
    const esp_err_t web_err = sp01::web_hmi_start(web, fill_hmi_snapshot, hmi_cal_zero, hmi_cal_span,
                                                  hmi_bench_do_pulse, hmi_bench_do_off);
    if (web_err != ESP_OK) ESP_LOGW(kTag, "HMI disabled: %s", esp_err_to_name(web_err));
}
