// Virtual bench controller task.
//
// Runs the real SP01 state machine (components/controller) against virtual
// digital inputs from the browser and a simulated weight. No physical input is
// read and no physical output is driven: the TCA9554 stays in the safe state
// established during the G1 self-test.
//
// The weight model is deliberately crude. It exists to exercise the FSM and
// reject routing, not to represent plant filling accuracy or to tune production
// detector thresholds.

#include "sdkconfig.h"

#ifdef CONFIG_SP01_VIRTUAL_IO

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sp01/controller.hpp"
#include "sp01/model.hpp"
#include "sp01/vio_web.hpp"

namespace sp01 {
namespace {

constexpr char kTag[] = "vbench";
constexpr uint32_t kTickMs = 20;

// Simulation-only rates and reject detector settings. These values are NOT
// production commissioning values; G4/G8 own the real thresholds.
constexpr float kCoarseRate = 8.0F;
constexpr float kFineRate = 1.2F;
constexpr float kBrokenBagNetRate = -3.0F;
constexpr float kVirtualLossTripKg = 0.25F;
constexpr uint64_t kVirtualLossPersistUs = 100000ULL;
constexpr uint64_t kVirtualRejectWaitTimeoutUs = 5000000ULL;

bool zeroing_state(State s) {
    return s == State::WaitPermissive || s == State::WaitFillPosition ||
           s == State::BagAcquire || s == State::TareReady;
}

void controller_task(void*) {
    ControllerConfig config{};
    config.broken_bag_loss_trip_kg = kVirtualLossTripKg;
    config.broken_bag_persist_us = kVirtualLossPersistUs;
    config.reject_wait_timeout_us = kVirtualRejectWaitTimeoutUs;
    Controller controller{config};
    const ControllerConfig cfg = controller.config();

    float net_kg = 0.0F;
    uint32_t sequence = 0;
    State prev_state = State::WaitPermissive;
    uint64_t stable_since_us = 0;
    bool broken_bag_sim = false;
    bool reject_window_pulse = false;

    ESP_LOGW(kTag, "virtual bench running: simulated weight, no physical IO");
    ESP_LOGW(kTag, "reject detector uses SIMULATION-ONLY thresholds");
    ESP_LOGI(kTag, "target=%.2f kg coarse_to_fine=%.2f kg",
             static_cast<double>(cfg.target_kg),
             static_cast<double>(cfg.coarse_to_fine_kg));

    for (;;) {
        const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());

        InputImage inputs{};
        const uint8_t bits = vio_di();
        for (size_t i = 0; i < 8; ++i) {
            inputs.di[i] = ((bits >> i) & 1U) != 0U;
        }
        inputs.mode = input(inputs, Di::MachineMotorRunning)
                          ? OperationMode::Auto
                          : OperationMode::Manual;

        switch (vio_take_command()) {
            case VioCommand::Reset:
                controller.reset(now_us);
                net_kg = 0.0F;
                broken_bag_sim = false;
                reject_window_pulse = false;
                ESP_LOGI(kTag, "controller reset by operator");
                break;
            case VioCommand::ClearFault:
                if (controller.clear_fault(now_us, inputs)) {
                    ESP_LOGI(kTag, "fault cleared");
                } else {
                    ESP_LOGW(kTag, "fault not clearable in current conditions");
                }
                break;
            case VioCommand::BrokenBagOn:
                broken_bag_sim = true;
                ESP_LOGW(kTag, "SIM broken-bag mass loss ON");
                break;
            case VioCommand::BrokenBagOff:
                broken_bag_sim = false;
                ESP_LOGI(kTag, "SIM broken-bag mass loss OFF");
                break;
            case VioCommand::RejectWindow:
                reject_window_pulse = true;
                ESP_LOGW(kTag, "SIM 210-degree reject window pulse");
                break;
            case VioCommand::None:
                break;
        }

        const State state = controller.snapshot().state;
        float rate = 0.0F;
        if (state == State::CoarseFill) rate = kCoarseRate;
        else if (state == State::FineFill) rate = kFineRate;
        if (broken_bag_sim &&
            (state == State::CoarseFill || state == State::FineFill)) {
            rate = kBrokenBagNetRate;
        }

        if (zeroing_state(state) && !zeroing_state(prev_state)) {
            net_kg = 0.0F;
        }
        if (rate != 0.0F) {
            net_kg += rate * (static_cast<float>(kTickMs) / 1000.0F);
            if (net_kg < 0.0F) net_kg = 0.0F;
            stable_since_us = now_us;
        }
        prev_state = state;

        WeightSnapshot weight{};
        weight.net_kg = net_kg;
        weight.sample_time_us = now_us;
        weight.sequence = ++sequence;
        weight.quality = WeightQuality::Good;
        weight.stable = (now_us - stable_since_us) > 200000ULL;

        PositionSnapshot position{};
        position.reject_window = reject_window_pulse;
        reject_window_pulse = false;

        const ControllerSnapshot snap = controller.tick(now_us, inputs, weight, position);

        if (snap.state != state) {
            ESP_LOGI(kTag, "%s -> %s fault=%s disposition=%s weight=%.3f kg cycle=%lu",
                     state_name(state), state_name(snap.state),
                     fault_name(snap.fault), disposition_name(snap.disposition),
                     static_cast<double>(net_kg),
                     static_cast<unsigned long>(snap.cycle_id));
        }

        uint8_t do_bits = 0;
        for (size_t i = 0; i < 8; ++i) {
            if (snap.outputs.channels[i]) do_bits |= static_cast<uint8_t>(1U << i);
        }

        VioStatus status{};
        status.do_bits = do_bits;
        status.state = state_name(snap.state);
        status.fault = fault_name(snap.fault);
        status.mode = mode_name(snap.mode);
        status.disposition = disposition_name(snap.disposition);
        status.weight_kg = net_kg;
        status.target_kg = cfg.target_kg;
        status.cycle_id = snap.cycle_id;
        status.broken_bag_sim = broken_bag_sim;
        status.reject_window = position.reject_window;
        status.broken_bag_detected_us = snap.broken_bag_detected_us;
        vio_publish(status);

        vTaskDelay(pdMS_TO_TICKS(kTickMs));
    }
}

}  // namespace

void vio_control_start() {
    xTaskCreate(controller_task, "vbench", 5120, nullptr, 4, nullptr);
}

}  // namespace sp01

#endif  // CONFIG_SP01_VIRTUAL_IO
