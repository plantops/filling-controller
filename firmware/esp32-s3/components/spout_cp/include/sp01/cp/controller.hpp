// SP01 filling controller — machine logic, no hardware in it.
// Sections referenced below are from SP01-mo-ta-may-v3.md.
#pragma once
#include <cstdint>
#include "sp01/cp/params.hpp"
#include "sp01/cp/angle.hpp"

namespace sp01::cp {

struct Inputs {
    bool  bag_present{false};   // PE-converter: air pressure in the holding rubber
    bool  prox_start{false};    // fixed switching vane, one pulse per revolution
    bool  prox_discharge{false};// fixed cylinder, extended only when downstream runs
    bool  air_active{false};    // 5.5-6 bar present
    bool  btn_start{false};
    bool  btn_stop{false};
    bool  btn_ack{false};       // clears a latched fault
    float weight_kg{0.f};
    bool  weight_valid{false};   // TLB delivered a fresh reading
    bool  turbine_overload{false}; // motor protection tripped; false when not wired
};

struct Outputs {
    bool feed_main{false};
    bool feed_dribble{false};
    bool bag_holder{false};
    bool bag_discharge{false};
    bool blow_out{false};
    bool aeration{false};
    bool turbine{false};
    bool alarm_lamp{false};
};

enum class State : std::uint8_t {
    Locked, Idle, WaitIndex, Clamp, BagVerify, NoBagEject,
    Tare, Coarse, Fine, Settle, WaitDischarge, Push, BagClear, Fault,
};

enum class Fault : std::uint8_t {
    None, AirLost, WeightLost, IndexLost, FillTimeout, BagBreak,
    OutOfTolerance, BagStuck, Stopped, TurbineOverload,
};

struct Result {
    State  state{State::Locked};
    Fault  fault{Fault::None};
    float  last_bag_kg{0.f};
    bool   last_bag_good{false};
    std::uint32_t bags_good{0};
    std::uint32_t bags_out_of_tol{0};
    std::uint32_t turbine_starts{0};   // one per bag by design; watch the contactor
};

class Controller {
public:
    Controller(const Params& p, const Recipe& r) : p_(p), r_(r), angle_(p) {}

    void set_recipe(const Recipe& r) { r_ = r; }   // refused mid-cycle by caller
    const Recipe& recipe() const { return r_; }

    // One control tick. now_us must be monotonic.
    Outputs tick(const Inputs& in, std::uint64_t now_us);

    const Result& result() const { return res_; }
    const AngleTracker& angle() const { return angle_; }

private:
    void enter(State s, std::uint64_t now_us) { res_.state = s; t_state_us_ = now_us; }
    void fail(Fault f, std::uint64_t now_us) { res_.fault = f; enter(State::Fault, now_us); }
    bool elapsed(std::uint32_t ms, std::uint64_t now_us) const {
        return now_us - t_state_us_ >= ms * 1000ull;
    }

    const Params& p_;
    Recipe        r_;
    AngleTracker  angle_;
    Result        res_{};
    std::uint64_t t_state_us_{0};
    bool          prox_start_prev_{false};
    float         tare_kg_{0.f};
    float         rate_ref_kg_{0.f};
    std::uint64_t rate_ref_us_{0};
    std::uint64_t slow_since_us_{0};
    std::uint64_t turbine_on_us_{0};
    bool          turbine_on_{false};
    bool          running_{false};
};

const char* to_string(State s);
const char* to_string(Fault f);

}  // namespace sp01::cp
