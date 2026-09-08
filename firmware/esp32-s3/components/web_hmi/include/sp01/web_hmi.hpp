#pragma once

#include "esp_err.h"
#include "sp01/model.hpp"
#include "sp01/tlb485.hpp"

#include <cstdint>

namespace sp01 {

struct HmiSnapshot {
    ControllerSnapshot controller{};
    InputImage inputs{};
    WeightSnapshot weight{};
    Tlb485Diagnostics tlb{};
    bool service_ready{false};
};

using HmiSnapshotFn = bool (*)(HmiSnapshot& out) noexcept;
using HmiCalZeroFn = esp_err_t (*)() noexcept;
using HmiCalSpanFn = esp_err_t (*)(float reference_kg) noexcept;

struct WebHmiConfig {
    const char* ssid{nullptr};
    const char* password{nullptr};
    const char* service_token{nullptr};
};

esp_err_t web_hmi_start(const WebHmiConfig& config,
                        HmiSnapshotFn snapshot_fn,
                        HmiCalZeroFn zero_fn,
                        HmiCalSpanFn span_fn) noexcept;

}  // namespace sp01
