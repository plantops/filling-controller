#include "sp01/controller.hpp"

#include <cmath>

namespace sp01 {
namespace {

bool filling_state(State state) noexcept {
    return state == State::CoarseFill || state == State::FineFill ||
           state == State::Cutoff || state == State::Settle ||
           state == State::WaitPushPosition;
}

}  // namespace

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
        case State::WaitPushPosition: return "WAIT_PUSH_POSITION";
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
    snapshot_.state_enter_us = now_us;
    snapshot_.outputs = safe_output_image();
    fill_position_armed_ = false;
    push_position_armed_ = false;
}

void Controller::force_fault(Fault code, std::uint64_t now_us) noexcept {
    fault(code, now_us);
    snapshot_.outputs = safe_output_image();
}

bool Controller::clear_fault(std::uint64_t now_us, const InputImage& inputs) noexcept {
    if (snapshot_.state != State::Fault) return true;
    if (machine_permissive(inputs)) return false;
    reset(now_us);
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
    if (weight.quality == WeightQuality::Fault) return false;
    if (weight.quality != WeightQuality::Good) return false;
    if (weight.sample_time_us > now_us) return false;
    return now_us - weight.sample_time_us <= config_.weight_stale_us;
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
        case State::WaitPushPosition:
            set_output(out, Do::ScannerDown, true);
            set_output(out, Do::BagDetectAir, true);
            break;
        case State::Push:
            set_output(out, Do::BagPush, true);
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

    if (filling_state(snapshot_.state) && !machine_permissive(inputs)) {
        fault(Fault::PermissiveLost, now_us);
    }
    if (filling_state(snapshot_.state) && !input(inputs, Di::BagPresent)) {
        fault(Fault::BagLost, now_us);
    }

    if (snapshot_.state != State::Fault) {
        switch (snapshot_.state) {
            case State::WaitPermissive:
                if (machine_permissive(inputs)) {
                    fill_position_armed_ = !input(inputs, Di::FillPosition);
                    transition(State::WaitFillPosition, now_us);
                }
                break;

            case State::WaitFillPosition:
                if (!machine_permissive(inputs)) transition(State::WaitPermissive, now_us);
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
                } else if (now_us - snapshot_.state_enter_us >= config_.settle_min_us && weight.stable) {
                    push_position_armed_ = !input(inputs, Di::PushPosition);
                    transition(State::WaitPushPosition, now_us);
                }
                break;

            case State::WaitPushPosition:
                if (!input(inputs, Di::PushPosition)) push_position_armed_ = true;
                if (push_position_armed_ && input(inputs, Di::PushPosition)) {
                    transition(State::Push, now_us);
                } else if (timed_out(now_us, config_.wait_push_timeout_us)) {
                    fault(Fault::StateTimeout, now_us);
                }
                break;

            case State::Push:
                if (now_us - snapshot_.state_enter_us >= config_.push_duration_us) {
                    transition(State::Complete, now_us);
                }
                break;

            case State::Complete:
                fill_position_armed_ = !input(inputs, Di::FillPosition);
                transition(machine_permissive(inputs) ? State::WaitFillPosition : State::WaitPermissive,
                           now_us);
                break;

            case State::Fault:
                break;
        }
    }

    snapshot_.outputs = outputs_for_state();
    return snapshot_;
}

}  // namespace sp01
