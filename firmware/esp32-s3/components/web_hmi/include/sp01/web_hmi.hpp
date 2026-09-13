#pragma once

// SP01 operator HMI server.
//
// This component renders and serves. It computes nothing: every derived value
// arrives from the application in HmiPublish, produced next to the controller by
// explain_controller(). The previous HMI kept a second copy of the permissive
// rule, the output map and the transition table in JavaScript, and it drifted
// out of agreement with the firmware without anyone noticing.

#include <cstdint>

#include "esp_err.h"
#include "sp01/controller.hpp"
#include "sp01/controller_explain.hpp"
#include "sp01/model.hpp"
#include "sp01/time_shift.hpp"

namespace sp01 {

struct HmiIdentity {
    char machine[24]{"—"};   // shown as-is, e.g. "MAY 3"
    char spout[24]{"—"};     // e.g. "VOI 7"
    char firmware[24]{""};   // short git sha
};

// PINs confirm an intent; they are not access control. Over plain HTTP on a
// commissioning access point, anyone on the network who knows the PIN can use
// it. The value is preventing a mis-tap from changing a running target.
struct HmiPins {
    char operator_pin[8]{"1111"};    // 4 digits, changes target
    char supervisor_pin[12]{"111111"};  // 6 digits, edits recipes
};

// One coherent picture of the machine, published by the application each tick.
struct HmiPublish {
    ControllerSnapshot snapshot{};
    Explain explain{};

    float weight_kg{0.0F};        // live net
    float target_kg{0.0F};        // in force now
    float target_pending_kg{0.0F};  // accepted, waiting for the spout to clear

    bool has_last_bag{false};
    float last_bag_kg{0.0F};
    char last_bag_time[12]{""};   // HH:MM:SS local

    bool time_synced{false};
    CivilTime civil{};
    std::uint8_t shift_no{0};
    ShiftCounters shift{};
    ShiftCounters day{};
    ShiftCounters unattributed{};
};

// Results the application returns when the HMI asks for a change.
enum class HmiResult : std::uint8_t { Ok = 0, BadPin, Locked, Rejected };

struct HmiCallbacks {
    // Stage a target change. The application applies it only once the current
    // bag has left the spout; it must not alter a bag mid-fill.
    HmiResult (*request_target)(float kg){nullptr};
    // Browser clock. tz_offset_min is minutes to add to UTC.
    void (*set_time)(std::uint64_t unix_ms, std::int16_t tz_offset_min){nullptr};
};

esp_err_t hmi_start(const HmiIdentity& identity, const HmiPins& pins,
                    const HmiCallbacks& callbacks);

void hmi_publish(const HmiPublish& state);

}  // namespace sp01
