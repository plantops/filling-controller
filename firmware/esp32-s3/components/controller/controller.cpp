#include "sp01/controller.hpp"

namespace sp01 {
namespace {

constexpr std::uint64_t kNormalizedDischargeCounts = 1000;

bool bag_required(State state) noexcept {
    return state == State::TareReady || state == State::CoarseFill ||
           state == State::FineFill || state == State::Cutoff ||
           state == State::Settle || state == State::WaitDischarge;
}

}  // namespace

const char* mode_name(OperationMode mode) noexcept {
    switch (mode) {
        case OperationMode::Auto: return "AUTO";
        case OperationMode::Manual: return "MANUAL";
    }
    return "UNKNOWN";
}

const char* state_name(State state) noexcept {
    switch (state) {
        case State::WaitPermissive: return "WAIT_PERMISSIVE";
        case State::WaitFillPosition: return "WAIT_FILL_POSITION";
        case State::BagAcquire: return "BAG_ACQUIRE";
        case State::BagVerify: return "BAG_VERIFY";
        case State::TareReady: return "TARE_READY";
        case State::CoarseFill: return "COARSE_FILL";
        case State::FineFill: return "FINE_FILL";
        case State::Cutoff: return "CUTOFF";
        case State::Settle: return "SETTLE";
        case State::WaitDischarge: return "WAIT_DISCHARGE";
        case State::Push: return "PUSH";
        case State::Complete: return "COMPLETE";
        case State::Fault: return "FAULT";
    }
    return "UNKNOWN";
}

const char* fault_name(Fault fault) noexcept {
    switch (fault) {
        case Fault::None: return "NONE";
        case Fault::PermissiveLost: return "PERMISSIVE_LOST";
        case Fault::BagMissing: return "BAG_MISSING";
        case Fault::BagLost: return "BAG_LOST";
        case Fault::WeightStale: return "WEIGHT_STALE";
        case Fault::WeightFault: return "WEIGHT_FAULT";
        case Fault::StateTimeout: return "STATE_TIMEOUT";
        case Fault::IoFault: return "IO_FAULT";
        case Fault::DischargeTimingInvalid: return "DISCHARGE_TIMING_INVALID";
        case Fault::ModeChanged: return "MODE_CHANGED";
    }
    return "UNKNOWN";
}

Controller::Controller(ControllerConfig config) noexcept : config_(config) {
    reset();
}

void Controller::reset(std::uint64_t now_us) noexcept {
    snapshot_ = {};
    snapshot_.state = State::WaitPermissive;
    snapshot_.fault = Fault::None;
    snapshot_.mode = OperationMode::Auto;
    snapshot_.state_enter_us = now_us;
    snapshot_.outputs = safe_output_image();
    fill_position_armed_ = false;
    discharge_ref_a_seen_ = false;
    prev_discharge_ref_a_ = false;
    prev_discharge_ref_b_ = false;
    discharge_ref_a_us_ = 0;
}

void Controller::force_fault(Fault code, std::uint64_t now_us) noexcept {
    fault(code, now_us);
    snapshot_.outputs = safe_output_image();
}

bool Controller::clear_fault(std::uint64_t now_us, const InputImage& inputs) noexcept {
    if (snapshot_.state != State::Fault) return true;
    if (input(inputs, Di::ProcessInitiative)) return false;
    reset(now_us);
    snapshot_.mode = inputs.mode;
    return true;
}

void Controller::transition(State next, std::uint64_t now_us) noexcept {
    if (snapshot_.state == next) return;
    snapshot_.state = next;
    snapshot_.state_enter_us = now_us;
    if (next == State::Complete) ++snapshot_.cycle_id;
}

void Controller::fault(Fault code, std::uint64_t now_us) noexcept {
    snapshot_.fault = code;
    transition(State::Fault, now_us);
}

bool Controller::timed_out(std::uint64_t now_us, std::uint64_t timeout_us) const noexcept {
    return timeout_us > 0 && now_us - snapshot_.state_enter_us >= timeout_us;
}

bool Controller::weight_fresh(std::uint64_t now_us, const WeightSnapshot& weight) const noexcept {
    if (weight.quality != WeightQuality::Good) return false;
    if (weight.sample_time_us > now_us) return false;
    return now_us - weight.sample_time_us <= config_.weight_stale_us;
}

bool Controller::idle_state() const noexcept {
    return snapshot_.state == State::WaitPermissive ||
           snapshot_.state == State::WaitFillPosition ||
           snapshot_.state == State::Complete;
}

bool Controller::manual_fill_state() const noexcept {
    return snapshot_.state == State::BagAcquire ||
           snapshot_.state == State::BagVerify ||
           snapshot_.state == State::TareReady ||
           snapshot_.state == State::CoarseFill ||
           snapshot_.state == State::FineFill ||
           snapshot_.state == State::Cutoff ||
           snapshot_.state == State::Settle;
}

bool Controller::auto_cycle_state() const noexcept {
    return snapshot_.state == State::BagAcquire ||
           snapshot_.state == State::BagVerify ||
           snapshot_.state == State::TareReady ||
           snapshot_.state == State::CoarseFill ||
           snapshot_.state == State::FineFill ||
           snapshot_.state == State::Cutoff ||
           snapshot_.state == State::Settle ||
           snapshot_.state == State::WaitDischarge ||
           snapshot_.state == State::Push;
}

void Controller::reset_discharge_capture(const InputImage& inputs) noexcept {
    discharge_ref_a_seen_ = false;
    discharge_ref_a_us_ = 0;
    snapshot_.discharge_ref_interval_us = 0;
    snapshot_.discharge_due_us = 0;
    prev_discharge_ref_a_ = input(inputs, Di::DischargeRefA);
    prev_discharge_ref_b_ = input(inputs, Di::DischargeRefB);
}

void Controller::update_discharge_capture(std::uint64_t now_us,
                                          const InputImage& inputs) noexcept {
    const bool ref_a = input(inputs, Di::DischargeRefA);
    const bool ref_b = input(inputs, Di::DischargeRefB);
    const bool ref_a_rise = ref_a && !prev_discharge_ref_a_;
    const bool ref_b_rise = ref_b && !prev_discharge_ref_b_;

    if (ref_a_rise) {
        discharge_ref_a_seen_ = true;
        discharge_ref_a_us_ = now_us;
        snapshot_.discharge_ref_interval_us = 0;
        snapshot_.discharge_due_us = 0;
    }

    if (ref_b_rise) {
        if (!discharge_ref_a_seen_ || now_us <= discharge_ref_a_us_ ||
            config_.discharge_lead_counts > config_.discharge_countdown_counts) {
            fault(Fault::DischargeTimingInvalid, now_us);
        } else {
            const std::uint64_t interval_us = now_us - discharge_ref_a_us_;
            const std::uint64_t countdown_counts =
                static_cast<std::uint64_t>(config_.discharge_countdown_counts -
                                           config_.discharge_lead_counts);
            const std::uint64_t delay_us =
                (interval_us * countdown_counts) / kNormalizedDischargeCounts;

            snapshot_.discharge_ref_interval_us = interval_us;
            snapshot_.discharge_due_us = now_us + delay_us;
            discharge_ref_a_seen_ = false;
        }
    }

    prev_discharge_ref_a_ = ref_a;
    prev_discharge_ref_b_ = ref_b;
}

OutputImage Controller::outputs_for_state() const noexcept {
    OutputImage out{};
    switch (snapshot_.state) {
        case State::BagAcquire:
        case State::BagVerify:
        case State::TareReady:
            set_output(out, Do::ScannerDown, true);
            set_output(out, Do::BagDetectAir, true);
            break;

        case State::CoarseFill:
            set_output(out, Do::ScannerDown, true);
            set_output(out, Do::BagDetectAir, true);
            set_output(out, Do::DosingValveA, true);
            set_output(out, Do::DosingValveB, true);
            set_output(out, Do::DosingValveC, true);
            set_output(out, Do::FillingMotor, true);
            set_output(out, Do::SpoutAeration, true);
            break;

        case State::FineFill:
            set_output(out, Do::ScannerDown, true);
            set_output(out, Do::BagDetectAir, true);
            set_output(out, Do::DosingValveA, true);
            set_output(out, Do::DosingValveC, true);
            set_output(out, Do::FillingMotor, true);
            set_output(out, Do::SpoutAeration, true);
            break;

        case State::Cutoff:
        case State::Settle:
        case State::WaitDischarge:
            set_output(out, Do::ScannerDown, true);
            set_output(out, Do::BagDetectAir, true);
            break;

        case State::Push:
            if (snapshot_.mode == OperationMode::Auto) {
                set_output(out, Do::BagPush, true);
            }
            break;

        case State::WaitPermissive:
        case State::WaitFillPosition:
        case State::Complete:
        case State::Fault:
            break;
    }
    return out;
}

ControllerSnapshot Controller::tick(std::uint64_t now_us,
                                    const InputImage& inputs,
                                    const WeightSnapshot& weight) noexcept {
    if (snapshot_.state == State::Fault) {
        snapshot_.outputs = safe_output_image();
        return snapshot_;
    }

    if (inputs.mode != snapshot_.mode) {
        if (!idle_state()) {
            fault(Fault::ModeChanged, now_us);
        } else {
            snapshot_.mode = inputs.mode;
            transition(State::WaitPermissive, now_us);
            fill_position_armed_ = false;
        }
    }

    if (snapshot_.state == State::Fault) {
        snapshot_.outputs = safe_output_image();
        return snapshot_;
    }

    if (snapshot_.mode == OperationMode::Manual && manual_fill_state()) {
        if (!input(inputs, Di::ProcessInitiative)) {
            snapshot_.fault = Fault::None;
            transition(State::WaitPermissive, now_us);
        } else if (!input(inputs, Di::HopperFeederRunning)) {
            fault(Fault::PermissiveLost, now_us);
        }
    }

    if (snapshot_.mode == OperationMode::Auto && auto_cycle_state() &&
        !auto_permissive(inputs)) {
        fault(Fault::PermissiveLost, now_us);
    }

    if (snapshot_.state != State::Fault && bag_required(snapshot_.state) &&
        !input(inputs, Di::BagPresent)) {
        fault(Fault::BagLost, now_us);
    }

    if (snapshot_.state != State::Fault) {
        switch (snapshot_.state) {
            case State::WaitPermissive:
                if (snapshot_.mode == OperationMode::Auto) {
                    if (auto_permissive(inputs)) {
                        fill_position_armed_ = !input(inputs, Di::FillPosition);
                        transition(State::WaitFillPosition, now_us);
                    }
                } else if (manual_fill_requested(inputs)) {
                    transition(State::BagAcquire, now_us);
                }
                break;

            case State::WaitFillPosition:
                if (snapshot_.mode != OperationMode::Auto) {
                    transition(State::WaitPermissive, now_us);
                    break;
                }
                if (!auto_permissive(inputs)) {
                    transition(State::WaitPermissive, now_us);
                    break;
                }
                if (!input(inputs, Di::FillPosition)) fill_position_armed_ = true;
                if (fill_position_armed_ && input(inputs, Di::FillPosition)) {
                    transition(State::BagAcquire, now_us);
                }
                break;

            case State::BagAcquire:
                if (input(inputs, Di::BagPresent)) {
                    transition(State::BagVerify, now_us);
                } else if (timed_out(now_us, config_.bag_acquire_timeout_us)) {
                    fault(Fault::BagMissing, now_us);
                }
                break;

            case State::BagVerify:
                if (!input(inputs, Di::BagPresent)) {
                    fault(Fault::BagMissing, now_us);
                } else {
                    transition(State::TareReady, now_us);
                }
                break;

            case State::TareReady:
                if (weight.quality == WeightQuality::Fault) {
                    fault(Fault::WeightFault, now_us);
                } else if (!weight_fresh(now_us, weight)) {
                    fault(Fault::WeightStale, now_us);
                } else {
                    transition(State::CoarseFill, now_us);
                }
                break;

            case State::CoarseFill:
                if (weight.quality == WeightQuality::Fault) {
                    fault(Fault::WeightFault, now_us);
                } else if (!weight_fresh(now_us, weight)) {
                    fault(Fault::WeightStale, now_us);
                } else if (weight.net_kg >= config_.coarse_to_fine_kg) {
                    transition(State::FineFill, now_us);
                } else if (timed_out(now_us, config_.coarse_timeout_us)) {
                    fault(Fault::StateTimeout, now_us);
                }
                break;

            case State::FineFill: {
                const float cutoff_kg = config_.target_kg - config_.cutoff_margin_kg;
                if (weight.quality == WeightQuality::Fault) {
                    fault(Fault::WeightFault, now_us);
                } else if (!weight_fresh(now_us, weight)) {
                    fault(Fault::WeightStale, now_us);
                } else if (weight.net_kg >= cutoff_kg) {
                    transition(State::Cutoff, now_us);
                } else if (timed_out(now_us, config_.fine_timeout_us)) {
                    fault(Fault::StateTimeout, now_us);
                }
                break;
            }

            case State::Cutoff:
                transition(State::Settle, now_us);
                break;

            case State::Settle:
                if (weight.quality == WeightQuality::Fault) {
                    fault(Fault::WeightFault, now_us);
                } else if (!weight_fresh(now_us, weight)) {
                    fault(Fault::WeightStale, now_us);
                } else if (now_us - snapshot_.state_enter_us >= config_.settle_min_us &&
                           weight.stable) {
                    if (snapshot_.mode == OperationMode::Manual) {
                        transition(State::Complete, now_us);
                    } else {
                        reset_discharge_capture(inputs);
                        transition(State::WaitDischarge, now_us);
                    }
                }
                break;

            case State::WaitDischarge:
                if (snapshot_.mode != OperationMode::Auto) {
                    fault(Fault::ModeChanged, now_us);
                    break;
                }
                update_discharge_capture(now_us, inputs);
                if (snapshot_.state != State::Fault && snapshot_.discharge_due_us != 0 &&
                    now_us >= snapshot_.discharge_due_us) {
                    transition(State::Push, now_us);
                } else if (snapshot_.state != State::Fault &&
                           timed_out(now_us, config_.wait_discharge_timeout_us)) {
                    fault(Fault::StateTimeout, now_us);
                }
                break;

            case State::Push:
                if (snapshot_.mode != OperationMode::Auto) {
                    fault(Fault::ModeChanged, now_us);
                } else if (now_us - snapshot_.state_enter_us >= config_.push_duration_us) {
                    transition(State::Complete, now_us);
                }
                break;

            case State::Complete:
                if (snapshot_.mode == OperationMode::Manual) {
                    if (!input(inputs, Di::ProcessInitiative)) {
                        transition(State::WaitPermissive, now_us);
                    }
                } else {
                    fill_position_armed_ = !input(inputs, Di::FillPosition);
                    transition(auto_permissive(inputs) ? State::WaitFillPosition
                                                       : State::WaitPermissive,
                               now_us);
                }
                break;

            case State::Fault:
                break;
        }
    }

    snapshot_.outputs = outputs_for_state();
    return snapshot_;
}

}  // namespace sp01
