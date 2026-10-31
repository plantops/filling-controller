#pragma once

#include "esp_err.h"
#include "sp01/model.hpp"
#include "sp01/tlb485.hpp"

#include <cstdint>

namespace sp01 {

struct HmiSnapshot {
    ControllerSnapshot controller{};
    InputImage inputs{};
    OutputImage commanded_outputs{};
    WeightSnapshot weight{};
    Tlb485Diagnostics tlb{};
    bool service_ready{false};
};

using HmiSnapshotFn = bool (*)(HmiSnapshot& out) noexcept;
using HmiCalZeroFn = esp_err_t (*)() noexcept;
using HmiCalSpanFn = esp_err_t (*)(float reference_kg) noexcept;
using HmiBenchDoPulseFn = esp_err_t (*)(std::uint8_t channel_1_to_8,
                                        std::uint32_t pulse_ms) noexcept;
using HmiBenchDoOffFn = esp_err_t (*)() noexcept;

struct WebHmiConfig {
    const char* ssid{nullptr};
    const char* password{nullptr};
    const char* service_token{nullptr};
    bool bench_do_enabled{false};
};

esp_err_t web_hmi_start(const WebHmiConfig& config,
                        HmiSnapshotFn snapshot_fn,
                        HmiCalZeroFn zero_fn,
                        HmiCalSpanFn span_fn,
                        HmiBenchDoPulseFn bench_do_pulse_fn,
                        HmiBenchDoOffFn bench_do_off_fn) noexcept;

}  // namespace sp01
