#pragma once

// Derived truth for the HMI, computed in firmware next to the controller.
//
// The previous HMI kept its own copy of the permissive rule, the output map and
// the transition table in JavaScript. When the controller changed, the screen
// silently lied. Everything the operator or commissioning engineer reads is
// computed here, from the same config and the same inputs the controller uses,
// and the browser only renders it.
//
// Pure logic, no ESP-IDF dependency, so the host test suite covers it.

#include <cstdint>

#include "sp01/controller.hpp"
#include "sp01/model.hpp"

namespace sp01 {

// Reasons the controller cannot proceed. Several can hold at once, so these are
// bit positions and the HMI shows a list, not a single string.
enum class Block : std::uint16_t {
    FeederNotRunning = 0,
    DownstreamNotReady,
    MotorNotRunning,
    NoInitiative,
    WaitingFillPosition,
    WaitingBag,
    WeightStale,
    WeightFault,
    WeightMoving,
    PositionInvalid,
    WaitingPushAngle,
    Faulted,
    Count,
};

// Human text, English and Vietnamese. The operator screen shows the remedy in
// Vietnamese because that is who reads it on the machine.
const char* block_name(Block block) noexcept;
const char* block_text_en(Block block) noexcept;
const char* block_text_vi(Block block) noexcept;

struct Explain {
    bool permissive{false};
    bool weight_ready{false};
    bool ready{false};              // nothing is blocking right now
    std::uint16_t blocks{0};        // bitmask of Block
    State next_state{State::WaitPermissive};
    std::uint8_t desired_mask{0};   // outputs the FSM requests, from the FSM
    float push_angle_deg{0.0F};
    float angle_to_push_deg{0.0F};  // forward distance remaining, -1 if unknown

    bool blocked_by(Block b) const noexcept {
        return (blocks & static_cast<std::uint16_t>(
                             1U << static_cast<std::uint16_t>(b))) != 0U;
    }
};

// `snapshot` must be the controller's current snapshot; the other arguments are
// the same images passed to Controller::tick on this cycle.
Explain explain_controller(const ControllerSnapshot& snapshot,
                           const ControllerConfig& config,
                           const InputImage& inputs,
                           const WeightSnapshot& weight,
                           const PositionSnapshot& position,
                           std::uint64_t now_us) noexcept;

}  // namespace sp01
