#pragma once

// Watches the input signals for combinations that cannot be true on a working
// machine, and blocks operation until somebody has looked.
//
// The controller already refuses to act on inputs it considers unsafe. What it
// does not do is notice that the inputs themselves are lying: two position
// sensors reading at once, an index pulse arriving while the main motor is
// reported stopped, a contact that has not moved in three revolutions. Those are
// wiring, sensor or configuration faults, and without a check for them the plant
// runs on wrong information and the fault surfaces later as something else.
//
// Only contradictions that hold regardless of machine detail are implemented
// here. Rules that depend on how this particular packer behaves belong in the
// machine profile and must be confirmed before being added.
//
// Pure logic, no ESP-IDF dependency, covered by the host test suite.

#include <cstdint>

#include "sp01/model.hpp"

namespace sp01 {

enum class Implausible : std::uint8_t {
    None = 0,
    // S4 sits at 315 deg; the nearest other sensor is 25 deg away, about one
    // second at rated speed. Both lines high at once means shorted wiring, or
    // both sets landed on the same input.
    PositionIndexAndMarkTogether,
    // The shaft cannot turn while the main motor is reported stopped.
    IndexWhileMotorStopped,
    // The motor is running but no index pulse has arrived for longer than a
    // revolution: dead sensor, broken coupling, or a stopped shaft.
    IndexMissingWhileRunning,
    // A position input is a pulse. Held high for more than a revolution it is
    // stuck: jammed target, welded contact, or a short.
    PositionInputStuckHigh,
    // The machine is turning but a position input has produced no edge at all
    // over several revolutions.
    PositionInputDead,
    // DI5 is a pulse at the fill position, not a level.
    FillPositionStuckHigh,
    // Confirmed by the plant owner: cement must not enter the bag while the
    // hopper feeder is stopped. Weight rising then means the feeder signal is
    // wrong, or material is passing a closed gate.
    WeightRisingWithFeederOff,
    Count,
};

const char* implausible_name(Implausible code) noexcept;
const char* implausible_text_en(Implausible code) noexcept;
const char* implausible_text_vi(Implausible code) noexcept;

struct PlausibilityConfig {
    std::uint64_t max_revolution_us{28800000};   // half rated speed
    std::uint32_t dead_input_revolutions{3};
    // An edge shorter than this is contact bounce, not a real transition.
    std::uint64_t debounce_us{5000};

    // Weight must climb by at least this much, for at least this long, with the
    // feeder off, before it counts. Generous on purpose: a rule that trips on
    // scale noise teaches people to ignore alarms.
    float feeder_off_rise_kg{0.5F};
    std::uint64_t feeder_off_window_us{1000000};
};

struct PlausibilityStatus {
    bool ready{true};              // no rule is currently violated
    std::uint16_t flags{0};        // bitmask of Implausible
    Implausible first{Implausible::None};
    std::uint64_t first_seen_us{0};

    bool violated(Implausible c) const noexcept {
        return (flags & static_cast<std::uint16_t>(
                            1U << static_cast<std::uint16_t>(c))) != 0U;
    }
};

class PlausibilityMonitor {
   public:
    PlausibilityMonitor() = default;
    explicit PlausibilityMonitor(const PlausibilityConfig& cfg) noexcept
        : cfg_(cfg) {}

    // Latches violations. Operation stays blocked until clear() is called, so a
    // fault that appears for one tick cannot be missed.
    void update(std::uint64_t now_us, const InputImage& inputs,
                const WeightSnapshot& weight) noexcept;

    // Called when the operator has investigated. Anything still wrong latches
    // again on the next tick, so clearing does not hide a live fault.
    void clear() noexcept;

    void reset() noexcept;

    const PlausibilityStatus& status() const noexcept { return status_; }
    bool ready() const noexcept { return status_.ready; }

   private:
    void raise(Implausible code, std::uint64_t now_us) noexcept;

    PlausibilityConfig cfg_{};
    PlausibilityStatus status_{};

    bool primed_{false};
    bool prev_index_{false};
    bool prev_mark_{false};
    bool prev_motor_{false};

    std::uint64_t index_high_since_us_{0};
    std::uint64_t mark_high_since_us_{0};
    std::uint64_t last_index_edge_us_{0};
    std::uint64_t last_mark_edge_us_{0};
    std::uint64_t motor_started_us_{0};
    std::uint64_t fill_pos_high_since_us_{0};
    bool prev_fill_pos_{false};
    bool prev_feeder_{false};
    float feeder_off_ref_kg_{0.0F};
    std::uint64_t feeder_off_since_us_{0};
};

}  // namespace sp01
