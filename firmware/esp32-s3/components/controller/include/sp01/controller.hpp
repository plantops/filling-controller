#pragma once

#include "sp01/model.hpp"

namespace sp01 {

class Controller {
public:
    explicit Controller(ControllerConfig config = {}) noexcept;

    [[nodiscard]] ControllerSnapshot tick(std::uint64_t now_us,
                                          const InputImage& inputs,
                                          const WeightSnapshot& weight) noexcept;

    void reset(std::uint64_t now_us = 0) noexcept;
    bool clear_fault(std::uint64_t now_us, const InputImage& inputs) noexcept;
    void force_fault(Fault code, std::uint64_t now_us) noexcept;

    [[nodiscard]] const ControllerSnapshot& snapshot() const noexcept { return snapshot_; }
    [[nodiscard]] const ControllerConfig& config() const noexcept { return config_; }

private:
    ControllerConfig config_{};
    ControllerSnapshot snapshot_{};

    bool fill_position_armed_{false};
    bool discharge_ref_a_seen_{false};
    bool prev_discharge_ref_a_{false};
    bool prev_discharge_ref_b_{false};
    std::uint64_t discharge_ref_a_us_{0};

    void transition(State next, std::uint64_t now_us) noexcept;
    void fault(Fault code, std::uint64_t now_us) noexcept;
    void reset_discharge_capture(const InputImage& inputs) noexcept;
    void update_discharge_capture(std::uint64_t now_us, const InputImage& inputs) noexcept;

    [[nodiscard]] bool timed_out(std::uint64_t now_us, std::uint64_t timeout_us) const noexcept;
    [[nodiscard]] bool weight_fresh(std::uint64_t now_us, const WeightSnapshot& weight) const noexcept;
    [[nodiscard]] bool idle_state() const noexcept;
    [[nodiscard]] bool manual_fill_state() const noexcept;
    [[nodiscard]] bool auto_cycle_state() const noexcept;
    [[nodiscard]] OutputImage outputs_for_state() const noexcept;
};

}  // namespace sp01
