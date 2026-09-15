// Every test below is a line from SP01-mo-ta-may-v3.md, not a line from the code.
#include "sp01/cp/controller.hpp"
#include "sp01/cp/io_map.hpp"
#include <cstdio>
#include <cstdlib>

using namespace sp01::cp;

static int failures = 0;
static void check(bool ok, const char* what) {
    std::printf("%-58s %s\n", what, ok ? "pass" : "FAIL");
    if (!ok) ++failures;
}

namespace {

constexpr Recipe kR50{"50.1", 50.0f, 42.0f, 0.20f, 0.20f};

// A rig that runs the controller at 10 ms ticks and lets a test move inputs,
// weight and the index vane the way the machine would.
struct Rig {
    Params  p{};
    Controller c{p, kR50};
    Inputs  in{};
    Outputs out{};
    std::uint64_t t{0};

    Rig() { in.air_active = true; in.weight_valid = true; }

    void step(std::uint32_t ms = 10) {
        for (std::uint32_t i = 0; i < ms; i += 10) { t += 10000; out = c.tick(in, t); }
    }
    // One vane pass: 200 ms of prox.start ON, then the rest of the revolution.
    void index_pulse() { in.prox_start = true; step(200); in.prox_start = false; step(10); }
    void revolution()  { index_pulse(); step(14800); }
    void ack()         { in.btn_ack = true; step(20); in.btn_ack = false; step(10); }
    State st() const   { return c.result().state; }
    Fault ft() const   { return c.result().fault; }

    // Drive a whole good bag from Idle, leaving the controller wherever it lands.
    void good_bag() {
        in.btn_start = true; step(20); in.btn_start = false;
        index_pulse();                    // -> Clamp
        step(400);                        // blow-out done -> BagVerify
        in.bag_present = true; step(50);  // -> Tare
        step(500);                        // tare settles -> Coarse
        for (int i = 0; i < 60 && st() == State::Coarse; ++i) { in.weight_kg += 1.0f; step(100); }
        for (int i = 0; i < 60 && st() == State::Fine;  ++i) { in.weight_kg += 0.5f; step(100); }
        in.weight_kg = 50.0f;             // after-flow lands
        step(500);                        // -> settle -> evaluate
    }
};

}  // namespace

int main() {
    // Section 7, rule 4 -------------------------------------------------------
    {
        Rig r;
        r.step(1000);
        check(r.st() == State::Locked, "power-up stays locked until acknowledged");
        r.ack();
        check(r.st() == State::Idle, "acknowledge leaves lock");
    }

    // Section 7, rule 1 -------------------------------------------------------
    {
        Rig r; r.ack();
        r.in.btn_start = true; r.step(20); r.in.btn_start = false;
        r.index_pulse(); r.step(400);
        check(r.st() == State::BagVerify, "clamp then verify");
        bool fed = false;
        for (int i = 0; i < 300; ++i) { r.step(10); fed |= r.out.feed_main || r.out.feed_dribble; }
        check(!fed, "never feeds while no bag pressure is reported");
        check(r.st() == State::NoBagEject || r.st() == State::WaitIndex,
              "no bag ejects the spout instead of faulting");
    }

    // Section 5, steps 5 and 6 ------------------------------------------------
    {
        Rig r; r.ack(); r.good_bag();
        check(r.c.result().last_bag_kg > 49.8f && r.c.result().last_bag_kg < 50.2f,
              "good bag lands inside tolerance");
        check(r.st() == State::WaitDischarge, "good bag waits for the discharge prox");
    }

    // Section 7, rule 2 -------------------------------------------------------
    {
        // Machine still turning, downstream never gives permission.
        Rig r; r.ack(); r.good_bag();
        bool pushed = false;
        for (int i = 0; i < 3 && r.ft() == Fault::None; ++i) {
            r.revolution();
            pushed |= r.out.bag_discharge;
        }
        check(!pushed, "never pushes a bag while prox.discharge is absent");
        check(r.ft() == Fault::BagStuck, "waiting past the limit becomes a fault");
    }
    {
        // Machine stopped with a full bag on the spout: the honest diagnosis is
        // the lost position signal, not a stuck bag.
        Rig r; r.ack(); r.good_bag();
        r.step(23000);
        check(r.ft() == Fault::IndexLost, "a stopped machine reports lost position, not a stuck bag");
    }
    {
        Rig r; r.ack(); r.good_bag();
        r.in.prox_discharge = true; r.step(50);
        check(r.out.bag_discharge, "pushes as soon as the mechanical permission arrives");
    }

    // Section 7, rule 3 -------------------------------------------------------
    {
        Rig r; r.ack();
        r.in.btn_start = true; r.step(20); r.in.btn_start = false;
        r.index_pulse(); r.step(400); r.in.bag_present = true; r.step(600);
        for (int i = 0; i < 60 && r.st() == State::Coarse; ++i) { r.in.weight_kg += 1.0f; r.step(100); }
        for (int i = 0; i < 60 && r.st() == State::Fine;  ++i) { r.in.weight_kg += 0.5f; r.step(100); }
        r.in.weight_kg = 51.0f;   // 1 kg over, well outside +0.20
        r.step(500);
        check(r.ft() == Fault::OutOfTolerance, "overweight bag latches a fault");
        bool pushed = false;
        r.in.prox_discharge = true;
        for (int i = 0; i < 200; ++i) { r.step(10); pushed |= r.out.bag_discharge; }
        check(!pushed, "overweight bag is not discharged even with permission present");
    }

    // Section 6 ---------------------------------------------------------------
    {
        Rig r; r.ack();
        r.in.btn_start = true; r.step(20); r.in.btn_start = false;
        r.index_pulse(); r.step(400); r.in.bag_present = true; r.step(600);
        check(r.st() == State::Coarse, "reaches coarse fill");
        r.in.air_active = false; r.step(20);
        check(r.ft() == Fault::AirLost, "loss of compressed air faults");
        check(!r.out.feed_main && !r.out.feed_dribble, "fault closes the gate");
    }
    {
        Rig r; r.ack();
        r.in.btn_start = true; r.step(20); r.in.btn_start = false;
        r.index_pulse(); r.step(400); r.in.bag_present = true; r.step(600);
        r.in.weight_valid = false; r.step(20);
        check(r.ft() == Fault::WeightLost, "loss of the scale faults");
    }
    {
        Rig r; r.ack();
        r.in.btn_start = true; r.step(20); r.in.btn_start = false;
        r.index_pulse(); r.step(400); r.in.bag_present = true; r.step(600);
        r.step(13000);   // no weight change at all
        check(r.ft() == Fault::BagBreak || r.ft() == Fault::FillTimeout,
              "a bag that stops taking cement faults");
    }

    // Turbine contactor -------------------------------------------------------
    {
        Rig r; r.ack();
        r.in.btn_start = true; r.step(20); r.in.btn_start = false;
        r.index_pulse(); r.step(400);
        bool early = false;
        for (int i = 0; i < 50; ++i) { r.step(10); early |= r.out.turbine; }
        check(!early, "turbine stays off while the gate is shut");

        r.in.bag_present = true; r.step(600);          // -> Coarse, gate opens
        check(r.out.feed_main && !r.out.turbine, "gate opens before the impeller starts");
        r.step(300);
        check(r.out.turbine, "impeller starts after the gate, once the delay passes");

        for (int i = 0; i < 60 && r.st() == State::Coarse; ++i) { r.in.weight_kg += 1.0f; r.step(100); }
        check(r.st() == State::Fine && r.out.turbine, "impeller keeps turning through fine feed");
        for (int i = 0; i < 60 && r.st() == State::Fine; ++i) { r.in.weight_kg += 0.5f; r.step(100); }
        r.in.weight_kg = 50.0f; r.step(100);
        check(!r.out.turbine, "impeller stops when the gate shuts");
        check(r.c.result().turbine_starts == 1, "one contactor start per bag");
    }
    {
        Rig r; r.ack();
        r.in.btn_start = true; r.step(20); r.in.btn_start = false;
        r.index_pulse(); r.step(400); r.in.bag_present = true; r.step(600);
        r.step(300);
        check(r.out.turbine, "impeller running");
        r.in.turbine_overload = true; r.step(20);
        check(r.ft() == Fault::TurbineOverload, "motor protection trips into a fault");
        check(!r.out.turbine && !r.out.feed_main, "fault drops the impeller and shuts the gate");
    }

    // Angle tracker -----------------------------------------------------------
    {
        Rig r; r.ack();
        check(r.c.angle().quality() == AngleQuality::Unknown, "no angle before two index pulses");
        r.revolution(); r.revolution(); r.revolution();
        check(r.c.angle().quality() == AngleQuality::Usable, "steady revolutions give a usable angle");
        r.index_pulse(); r.step(30000);
        check(r.c.angle().quality() == AngleQuality::Lost, "a missing vane is reported as lost");
    }
    {
        Rig r; r.ack();
        r.revolution(); r.revolution();
        r.index_pulse(); r.step(9000); r.index_pulse();   // 40% short revolution
        check(r.c.angle().quality() == AngleQuality::Coasting,
              "a revolution outside the jitter limit downgrades the angle");
    }
    {
        Rig r; r.ack();
        r.in.prox_start = true; r.step(30); r.in.prox_start = false;  // 30 ms spike
        r.step(1000);
        check(r.c.angle().revolutions() == 0, "a spike shorter than the debounce is not a revolution");
    }

    // Terminal map -----------------------------------------------------------
    {
        std::array<bool, 8> image{};
        image[static_cast<std::size_t>(Di::BagPresent)] = true;
        image[static_cast<std::size_t>(Di::AirActive)]  = true;
        const Inputs in = inputs_from_di(image, 12.5f, true);
        check(in.bag_present && in.air_active && !in.prox_start,
              "DI terminals land on the right machine signals");
        check(in.weight_kg > 12.4f && in.weight_valid, "weight rides along with the terminals");

        Outputs out{};
        out.feed_dribble = true;
        out.turbine      = true;
        const auto phys = di_image_to_do(out);
        check(phys[static_cast<std::size_t>(Do::FeedDribble)] &&
              phys[static_cast<std::size_t>(Do::Turbine)] &&
              !phys[static_cast<std::size_t>(Do::FeedMain)],
              "DO terminals carry only the commanded outputs");
    }

    std::printf("\n%s\n", failures ? "TESTS FAILED" : "all tests pass");
    return failures ? 1 : 0;
}
