#include "sp01/cp/controller.hpp"

namespace sp01::cp {

const char* to_string(State s) {
    switch (s) {
        case State::Locked:       return "LOCKED";
        case State::Idle:         return "IDLE";
        case State::WaitIndex:    return "WAIT_INDEX";
        case State::Clamp:        return "CLAMP";
        case State::BagVerify:    return "BAG_VERIFY";
        case State::NoBagEject:   return "NO_BAG_EJECT";
        case State::Tare:         return "TARE";
        case State::Coarse:       return "COARSE";
        case State::Fine:         return "FINE";
        case State::Settle:       return "SETTLE";
        case State::WaitDischarge:return "WAIT_DISCHARGE";
        case State::Push:         return "PUSH";
        case State::BagClear:     return "BAG_CLEAR";
        case State::Fault:        return "FAULT";
    }
    return "?";
}

const char* to_string(Fault f) {
    switch (f) {
        case Fault::None:           return "NONE";
        case Fault::AirLost:        return "AIR_LOST";
        case Fault::WeightLost:     return "WEIGHT_LOST";
        case Fault::IndexLost:      return "INDEX_LOST";
        case Fault::FillTimeout:    return "FILL_TIMEOUT";
        case Fault::BagBreak:       return "BAG_BREAK";
        case Fault::OutOfTolerance: return "OUT_OF_TOLERANCE";
        case Fault::BagStuck:       return "BAG_STUCK";
        case Fault::Stopped:        return "STOPPED";
        case Fault::TurbineOverload:return "TURBINE_OVERLOAD";
    }
    return "?";
}

Outputs Controller::tick(const Inputs& in, std::uint64_t now_us) {
    angle_.update(in.prox_start, now_us);
    const bool index_rise = in.prox_start && !prox_start_prev_;
    prox_start_prev_ = in.prox_start;

    Outputs out{};
    const float net = in.weight_kg - tare_kg_;

    // ---- section 7, rule 4: power-up never resumes filling by itself --------
    if (res_.state == State::Locked) {
        out.alarm_lamp = true;
        if (in.btn_ack) { running_ = false; enter(State::Idle, now_us); }
        return out;
    }

    // ---- conditions that abort any state ------------------------------------
    const bool filling = res_.state == State::Coarse || res_.state == State::Fine;
    if (res_.state != State::Fault) {
        if (in.btn_stop)                              fail(Fault::Stopped, now_us);
        else if (!in.air_active)                      fail(Fault::AirLost, now_us);
        else if (filling && !in.weight_valid)         fail(Fault::WeightLost, now_us);
        else if (in.turbine_overload && turbine_on_) fail(Fault::TurbineOverload, now_us);
        else if (angle_.quality() == AngleQuality::Lost && res_.state != State::Idle)
                                                      fail(Fault::IndexLost, now_us);
    }

    // The impeller only ever turns while the gate is open, and only after it:
    // starting it against a closed gate churns the vessel for nothing.
    const bool gate_open = res_.state == State::Coarse || res_.state == State::Fine;
    if (!gate_open) { turbine_on_ = false; }

    switch (res_.state) {
    case State::Idle:
        if (in.btn_start) { running_ = true; enter(State::WaitIndex, now_us); }
        break;

    case State::WaitIndex:
        if (index_rise) enter(State::Clamp, now_us);
        break;

    case State::Clamp:
        out.bag_holder = true;
        out.blow_out   = !elapsed(p_.blow_out_ms.value, now_us);
        if (elapsed(p_.blow_out_ms.value, now_us)) enter(State::BagVerify, now_us);
        break;

    case State::BagVerify:
        out.bag_holder = true;
        if (in.bag_present)                                 enter(State::Tare, now_us);
        else if (elapsed(p_.bag_verify_ms.value, now_us))   enter(State::NoBagEject, now_us);
        break;

    // No bag, or a bag hung wrong: tilt the frame to clear the spout and wait
    // for the next revolution. Manual p.157 — this is not a fault.
    case State::NoBagEject:
        out.bag_discharge = !elapsed(p_.push_ms.value, now_us);
        if (elapsed(p_.push_ms.value, now_us)) enter(State::WaitIndex, now_us);
        break;

    case State::Tare:
        out.bag_holder = true;
        if (!in.weight_valid) { fail(Fault::WeightLost, now_us); break; }
        if (elapsed(p_.settle_ms.value, now_us)) {
            tare_kg_      = in.weight_kg;
            rate_ref_kg_  = 0.f;
            rate_ref_us_  = now_us;
            slow_since_us_= 0;
            enter(State::Coarse, now_us);
        }
        break;

    case State::Coarse:
    case State::Fine: {
        // ---- section 7, rule 1 ------------------------------------------------
        const bool may_feed = in.bag_present && in.air_active && in.weight_valid;
        // Pressure gone mid-fill means the bag burst or slipped off the spout.
        if (!may_feed) { fail(in.bag_present ? Fault::WeightLost : Fault::BagBreak, now_us); break; }

        out.bag_holder = true;
        out.aeration   = true;
        if (res_.state == State::Coarse) out.feed_main = true; else out.feed_dribble = true;

        // Gate first, impeller after: see turbine_start_delay_ms.
        if (!turbine_on_ && res_.state == State::Coarse &&
            elapsed(p_.turbine_start_delay_ms.value, now_us)) {
            turbine_on_    = true;
            turbine_on_us_ = now_us;
            ++res_.turbine_starts;
        }
        out.turbine = turbine_on_;

        // bag break: fill rate below the floor for longer than the delay
        if (now_us - rate_ref_us_ >= 100000ull) {
            const float dt_s = static_cast<float>(now_us - rate_ref_us_) / 1e6f;
            const float rate = (net - rate_ref_kg_) / dt_s;
            rate_ref_kg_ = net;
            rate_ref_us_ = now_us;
            if (rate < p_.bag_break_min_kg_s.value) {
                if (slow_since_us_ == 0) slow_since_us_ = now_us;
                if (now_us - slow_since_us_ >= p_.bag_break_delay_ms.value * 1000ull) {
                    fail(Fault::BagBreak, now_us); break;
                }
            } else {
                slow_since_us_ = 0;
            }
        }

        if (elapsed(p_.fill_timeout_ms.value, now_us)) { fail(Fault::FillTimeout, now_us); break; }

        if (res_.state == State::Coarse) {
            if (net >= r_.coarse_to_fine_kg) enter(State::Fine, now_us);
        } else {
            if (net >= r_.target_kg - p_.after_flow_kg.value) enter(State::Settle, now_us);
        }
        break;
    }

    case State::Settle:
        out.bag_holder = true;
        if (elapsed(p_.settle_ms.value, now_us)) {
            res_.last_bag_kg   = net;
            const bool good = net >= r_.target_kg - r_.tol_minus_kg &&
                              net <= r_.target_kg + r_.tol_plus_kg;
            res_.last_bag_good = good;
            if (good) { ++res_.bags_good; enter(State::WaitDischarge, now_us); }
            // ---- section 7, rule 3: out of tolerance never leaves on its own ---
            else      { ++res_.bags_out_of_tol; fail(Fault::OutOfTolerance, now_us); }
        }
        break;

    // ---- section 7, rule 2: the push waits for the mechanical permission -----
    // prox.discharge is position AND downstream-ready in one signal. The angle
    // is only used to say which of the two is missing.
    case State::WaitDischarge:
        out.bag_holder = true;
        if (in.prox_discharge)                              enter(State::Push, now_us);
        else if (elapsed(p_.wait_discharge_ms.value, now_us)) fail(Fault::BagStuck, now_us);
        break;

    case State::Push:
        out.bag_discharge = true;
        out.bag_holder    = false;
        if (elapsed(p_.push_ms.value, now_us)) enter(State::BagClear, now_us);
        break;

    case State::BagClear:
        if (!in.bag_present)                                    enter(State::WaitIndex, now_us);
        else if (elapsed(p_.bag_clear_timeout_ms.value, now_us)) fail(Fault::BagStuck, now_us);
        break;

    case State::Fault:
        out.alarm_lamp = true;   // every output stays false: valves are spring-closed
        if (in.btn_ack && !in.btn_stop) {
            res_.fault = Fault::None;
            enter(running_ ? State::WaitIndex : State::Idle, now_us);
        }
        break;

    case State::Locked:
        break;
    }
    return out;
}

}  // namespace sp01::cp
