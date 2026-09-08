#include "sp01/controller.hpp"

#include <cstdint>
#include <limits>

namespace sp01 {
namespace {

bool active_bag_state(State state) noexcept {
    return state == State::CoarseFill || state == State::FineFill ||
           state == State::Cutoff || state == State::Settle ||
           state == State::WaitDischarge;
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
        case Fault::DischargeTimingInvalid: return "DISCHARGE_TIMING_INVALID";
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
    discharge_a_armed_ = false;
    discharge_b_armed_ = false;
    discharge_have_a_ = false;
    discharge_scheduled_ = false;
    discharge_a_us_ = 0;
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

bool Controller::weight_fresh(std::uint64_t now_us,
                              const WeightSnapshot& weight) const noexcept {
    if (weight.quality != WeightQuality::Good) return false;
    if (weight.sample_time_us > now_us) return false;
    return now_us - weight.sample_time_us <= config_.weight_stale_us;
}

void Controller::arm_discharge(const InputImage& inputs) noexcept {
    discharge_a_armed_ = !input(inputs, Di::DischargeRefA);
    discharge_b_armed_ = !input(inputs, Di::DischargeRefB);
    discharge_have_a_ = false;
    discharge_scheduled_ = false;
    discharge_a_us_ = 0;
    snapshot_.discharge_ref_interval_us = 0;
    snapshot_.discharge_command_due_us = 0;
}

bool Controller::update_discharge(std::uint64_t now_us,
                                  const InputImage& inputs) noexcept {
    const bool ref_a = input(inputs, Di::DischargeRefA);
    const bool ref_b = input(inputs, Di::DischargeRefB);

    if (!ref_a) discharge_a_armed_ = true;
    if (!discharge_have_a_ && discharge_a_armed_ && ref_a) {
        discharge_have_a_ = true;
        discharge_a_us_ = now_us;
        discharge_b_armed_ = !ref_b;
    }

    if (discharge_have_a_ && !ref_b) discharge_b_armed_ = true;

    if (discharge_have_a_ && !discharge_scheduled_ &&
        discharge_b_armed_ && ref_b) {
        const std::uint64_t ref_interval_us = now_us - discharge_a_us_;

        if (ref_interval_us == 0 ||
            config_.discharge_ref_span_deg <= 0.0F ||
            config_.discharge_target_after_b_deg < 0.0F) {
            fault(Fault::DischargeTimingInvalid, now_us);
            return false;
        }

        const double target_from_b_us_d =
            static_cast<double>(ref_interval_us) *
            static_cast<double>(config_.discharge_target_after_b_deg) /
            static_cast<double>(config_.discharge_ref_span_deg);

        if (target_from_b_us_d < 0.0 ||
            target_from_b_us_d >
                static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
            fault(Fault::DischargeTimingInvalid, now_us);
            return false;
        }

        const auto target_from_b_us =
            static_cast<std::uint64_t>(target_from_b_us_d + 0.5);

        if (target_from_b_us < config_.discharge_actuator_delay_us) {
            // Sensor B is too late for this speed and measured actuator lag.
            // Failing is safer than knowingly ejecting late.
            fault(Fault::DischargeTimingInvalid, now_us);
            return false;
        }

        snapshot_.discharge_ref_interval_us = ref_interval_us;
        snapshot_.discharge_command_due_us =
            now_us + (target_from_b_us - config_.discharge_actuator_delay_us);
        discharge_scheduled_ = true;
    }

    return discharge_scheduled_ &&
           now_us >= snapshot_.discharge_command_due_us;
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

    if (active_bag_state(snapshot_.state) && !machine_permissive(inputs)) {
        fault(Fault::PermissiveLost, now_us);
    }
    if (active_bag_state(snapshot_.state) && !input(inputs, Di::BagPresent)) {
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
                if (!machine_permissive(inputs)) {
                    transition(State::WaitPermissive, now_us);
                }
                if (!input(inputs, Di::FillPosition)) {
                    fill_position_armed_ = true;
                }
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
                const float cutoff_kg =
                    config_.target_kg - config_.cutoff_margin_kg;
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
                } else if (now_us - snapshot_.state_enter_us >=
                               config_.settle_min_us &&
                           weight.stable) {
                    arm_discharge(inputs);
                    transition(State::WaitDischarge, now_us);
                }
                break;

            case State::WaitDischarge:
                if (update_discharge(now_us, inputs)) {
                    transition(State::Push, now_us);
                } else if (snapshot_.state != State::Fault &&
                           timed_out(now_us,
                                     config_.wait_discharge_timeout_us)) {
                    fault(Fault::StateTimeout, now_us);
                }
                break;

            case State::Push:
                if (now_us - snapshot_.state_enter_us >=
                    config_.push_duration_us) {
                    transition(State::Complete, now_us);
                }
                break;

            case State::Complete:
                fill_position_armed_ = !input(inputs, Di::FillPosition);
                transition(machine_permissive(inputs)
                               ? State::WaitFillPosition
                               : State::WaitPermissive,
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
