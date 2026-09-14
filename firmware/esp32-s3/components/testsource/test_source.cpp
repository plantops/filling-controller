#include "sp01/test_source.hpp"

namespace sp01 {
namespace {

// States in which the simulated plant is actually putting cement in the bag.
float fill_rate(State s, const SimConfig& cfg) noexcept {
    if (s == State::CoarseFill) return cfg.coarse_rate_kg_s;
    if (s == State::FineFill) return cfg.fine_rate_kg_s;
    return 0.0F;
}

// Between bags the scale returns to zero.
bool zeroing_state(State s) noexcept {
    return s == State::WaitPermissive || s == State::WaitFillPosition ||
           s == State::BagAcquire || s == State::TareReady;
}

}  // namespace

const char* run_mode_name(RunMode mode) noexcept {
    switch (mode) {
        case RunMode::RealHw: return "REAL_HW";
        case RunMode::FullSw: return "FULL_SW";
        case RunMode::Simu: return "SIMU";
    }
    return "?";
}

const char* mode_change_name(ModeChange result) noexcept {
    switch (result) {
        case ModeChange::Ok: return "OK";
        case ModeChange::BadPin: return "BAD_PIN";
        case ModeChange::NotIdle: return "NOT_IDLE";
        case ModeChange::Unchanged: return "UNCHANGED";
    }
    return "?";
}

ModeChange TestSource::request_mode(RunMode next, State controller_state,
                                    bool pin_ok) noexcept {
    if (next == mode_) return ModeChange::Unchanged;
    if (!pin_ok) return ModeChange::BadPin;
    // Changing the source of truth mid-cycle would leave a bag started from one
    // source and finished from another.
    if (controller_state != State::WaitPermissive) return ModeChange::NotIdle;

    mode_ = next;
    if (next == RunMode::Simu) reset_sim();
    return ModeChange::Ok;
}

void TestSource::set_di(std::size_t channel, bool value) noexcept {
    if (channel < manual_inputs_.di.size()) manual_inputs_.di[channel] = value;
}

void TestSource::set_weight_kg(float kg) noexcept { manual_weight_kg_ = kg; }
void TestSource::set_angle_deg(float deg) noexcept {
    while (deg < 0.0F) deg += 360.0F;
    while (deg >= 360.0F) deg -= 360.0F;
    manual_angle_deg_ = deg;
}
void TestSource::set_position_valid(bool valid) noexcept {
    manual_position_valid_ = valid;
}

void TestSource::reset_sim() noexcept {
    sim_weight_kg_ = 0.0F;
    sim_angle_deg_ = 0.0F;
    sim_prev_state_ = State::WaitPermissive;
}

SourceImages TestSource::apply(std::uint64_t now_us, std::uint64_t dt_us,
                               const SourceImages& hw,
                               State controller_state) noexcept {
    if (mode_ == RunMode::RealHw) return hw;

    SourceImages out{};
    ++sequence_;

    if (mode_ == RunMode::FullSw) {
        out.inputs = manual_inputs_;
        out.inputs.mode = input(manual_inputs_, Di::MachineMotorRunning)
                              ? OperationMode::Auto
                              : OperationMode::Manual;
        out.weight.net_kg = manual_weight_kg_;
        out.weight.sample_time_us = now_us;
        out.weight.sequence = sequence_;
        out.weight.quality = WeightQuality::Good;
        out.weight.stable = true;  // held by hand, so it is by definition settled
        out.position.valid = manual_position_valid_;
        out.position.angle_deg = manual_angle_deg_;
        out.position.revolution_us = sim_.revolution_us;
        return out;
    }

    // Simu: the plant runs by itself.
    if (sim_running_) {
        sim_angle_deg_ += 360.0F * static_cast<float>(dt_us) /
                          static_cast<float>(sim_.revolution_us);
        while (sim_angle_deg_ >= 360.0F) sim_angle_deg_ -= 360.0F;

        const float rate = fill_rate(controller_state, sim_);
        if (zeroing_state(controller_state) && !zeroing_state(sim_prev_state_)) {
            sim_weight_kg_ = 0.0F;
        }
        sim_weight_kg_ += rate * static_cast<float>(dt_us) / 1000000.0F;
        sim_prev_state_ = controller_state;
    }

    // Permissives held true so cycles repeat; the bag arrives at fill position.
    const auto di = [&out](Di ch, bool v) {
        out.inputs.di[static_cast<std::size_t>(ch)] = v;
    };
    di(Di::HopperFeederRunning, true);
    di(Di::DownstreamConveyorReady, true);
    di(Di::MachineMotorRunning, true);
    di(Di::ProcessInitiative, true);
    di(Di::FillPosition, sim_angle_deg_ < 30.0F);
    di(Di::BagPresent, controller_state != State::WaitPermissive &&
                           controller_state != State::WaitFillPosition &&
                           controller_state != State::Complete);
    out.inputs.mode = OperationMode::Auto;

    out.weight.net_kg = sim_weight_kg_;
    out.weight.sample_time_us = now_us;
    out.weight.sequence = sequence_;
    out.weight.quality = WeightQuality::Good;
    out.weight.stable = fill_rate(controller_state, sim_) == 0.0F;

    out.position.valid = true;
    out.position.angle_deg = sim_angle_deg_;
    out.position.revolution_us = sim_.revolution_us;
    return out;
}

}  // namespace sp01
