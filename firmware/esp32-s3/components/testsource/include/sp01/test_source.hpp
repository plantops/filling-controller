#pragma once

// Where the controller's inputs come from.
//
//   RealHw  — DI, weight and position as read from the board.
//   FullSw  — the operator drives every input by hand, one step at a time, to
//             inspect controller logic. Nothing moves unless they move it.
//   Simu    — a crude plant model runs by itself so several bag cycles can be
//             watched in a row.
//
// Injection happens at the InputImage / WeightSnapshot / PositionSnapshot
// boundary. Raw GPIO is never faked, and the controller is never modified to
// make simulation convenient.
//
// Two rules are enforced here rather than left to the UI:
//
//   1. Physical output authority is impossible outside RealHw. FullSw and Simu
//      exist precisely for when nobody is standing at the machine, so there is
//      no setting that opens them up. Exercising a real valve is a separate
//      path, gated by CONFIG_SP01_BENCH_DO_TEST_ENABLE.
//   2. A mode change is refused unless the controller is at WAIT_PERMISSIVE and
//      the supervisor PIN is correct. Swapping the source of truth mid-cycle
//      would leave a bag half filled from one source and finished from another.
//
// Pure logic, no ESP-IDF dependency, covered by the host test suite.

#include <cstdint>

#include "sp01/model.hpp"

namespace sp01 {

enum class RunMode : std::uint8_t { RealHw = 0, FullSw, Simu };

const char* run_mode_name(RunMode mode) noexcept;

enum class ModeChange : std::uint8_t {
    Ok = 0,
    BadPin,
    NotIdle,       // controller is not at WAIT_PERMISSIVE
    Unchanged,
};

const char* mode_change_name(ModeChange result) noexcept;

struct SourceImages {
    InputImage inputs{};
    WeightSnapshot weight{};
    PositionSnapshot position{};
};

struct SimConfig {
    std::uint64_t revolution_us{14400000};  // 14.4 s per bag per spout
    float coarse_rate_kg_s{8.0F};
    float fine_rate_kg_s{1.2F};
};

class TestSource {
   public:
    RunMode mode() const noexcept { return mode_; }

    // True only in RealHw. FullSw and Simu can never drive a physical output.
    bool output_authority_possible() const noexcept {
        return mode_ == RunMode::RealHw;
    }

    ModeChange request_mode(RunMode next, State controller_state,
                            bool pin_ok) noexcept;

    // --- FullSw controls ---------------------------------------------------
    void set_di(std::size_t channel, bool value) noexcept;
    void set_weight_kg(float kg) noexcept;
    void set_angle_deg(float deg) noexcept;
    void set_position_valid(bool valid) noexcept;

    // --- Simu control ------------------------------------------------------
    void set_running(bool running) noexcept { sim_running_ = running; }
    bool running() const noexcept { return sim_running_; }
    void reset_sim() noexcept;

    // Produces the images the controller sees this tick. `hw` is what the board
    // actually read; it is passed through untouched in RealHw.
    SourceImages apply(std::uint64_t now_us, std::uint64_t dt_us,
                       const SourceImages& hw, State controller_state) noexcept;

    float manual_weight_kg() const noexcept { return manual_weight_kg_; }
    float manual_angle_deg() const noexcept { return manual_angle_deg_; }
    const InputImage& manual_inputs() const noexcept { return manual_inputs_; }

   private:
    RunMode mode_{RunMode::RealHw};

    InputImage manual_inputs_{};
    float manual_weight_kg_{0.0F};
    float manual_angle_deg_{0.0F};
    bool manual_position_valid_{true};

    SimConfig sim_{};
    bool sim_running_{true};
    float sim_weight_kg_{0.0F};
    float sim_angle_deg_{0.0F};
    State sim_prev_state_{State::WaitPermissive};
    std::uint32_t sequence_{0};
};

}  // namespace sp01
