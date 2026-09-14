#pragma once

// SP01 operator + commissioning HMI server.
//
// The HMI renders only values computed by the application/controller. It does
// not duplicate permissive, output-map or transition logic in JavaScript.

#include <cstdint>

#include "esp_err.h"
#include "sp01/controller.hpp"
#include "sp01/controller_explain.hpp"
#include "sp01/model.hpp"
#include "sp01/time_shift.hpp"

namespace sp01 {

struct HmiIdentity {
    char machine[24]{"—"};
    char spout[24]{"—"};
    char firmware[40]{""};
};

struct HmiPins {
    char operator_pin[8]{"1111"};
    char supervisor_pin[12]{"111111"};
};

// One coherent picture of the machine, published by the application each tick.
struct HmiPublish {
    ControllerSnapshot snapshot{};
    Explain explain{};

    // DEV HMI raw/live view. These are copied from the same control-loop tick
    // that produced snapshot/explain, so DI -> core -> DO cannot drift.
    InputImage inputs{};
    WeightSnapshot weight{};
    OutputImage commanded_outputs{};
    PositionSnapshot position{};
    bool shadow_mode{false};

    float weight_kg{0.0F};
    float target_kg{0.0F};
    float target_pending_kg{0.0F};

    bool has_last_bag{false};
    float last_bag_kg{0.0F};
    char last_bag_time[12]{""};

    bool time_synced{false};
    CivilTime civil{};
    std::uint8_t shift_no{0};
    ShiftCounters shift{};
    ShiftCounters day{};
    ShiftCounters unattributed{};
};

enum class HmiResult : std::uint8_t { Ok = 0, BadPin, Locked, Rejected };

struct HmiCallbacks {
    HmiResult (*request_target)(float kg){nullptr};
    void (*set_time)(std::uint64_t unix_ms, std::int16_t tz_offset_min){nullptr};
};

esp_err_t hmi_start(const HmiIdentity& identity, const HmiPins& pins,
                    const HmiCallbacks& callbacks);

void hmi_publish(const HmiPublish& state);

}  // namespace sp01
