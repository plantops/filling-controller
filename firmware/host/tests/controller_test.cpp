#include "sp01/controller.hpp"
#include "sp01/host_sim.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define REQUIRE(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); std::abort(); } } while (false)

namespace {

using sp01::Di;
using sp01::Do;
using sp01::Fault;
using sp01::OperationMode;
using sp01::State;
using sp01::WeightQuality;

void set_auto_permissive(sp01::host::VirtualIo& io, bool value = true) {
    io.set_input(Di::HopperFeederRunning, value);
    io.set_input(Di::DownstreamConveyorReady, value);
    io.set_input(Di::MachineMotorRunning, value);
    io.set_input(Di::ProcessInitiative, value);
}

void publish(sp01::host::VirtualWeigher& w,
             const sp01::host::ManualClock& c,
             float kg,
             bool stable = false,
             WeightQuality quality = WeightQuality::Good) {
    w.publish(kg, stable, quality, c.now_us());
}

// Simulated shaft. Angle advances with the clock so the controller sees the
// same position signal it will get from the decoder on the machine.
struct Shaft {
    bool valid{true};
    std::uint64_t revolution_us{14400000};
    float angle_deg{0.0F};

    sp01::PositionSnapshot snapshot() const {
        sp01::PositionSnapshot p{};
        p.valid = valid;
        p.angle_deg = angle_deg;
        p.revolution_us = revolution_us;
        return p;
    }
    void advance(std::uint64_t us) {
        angle_deg += 360.0F * static_cast<float>(us) /
                     static_cast<float>(revolution_us);
        while (angle_deg >= 360.0F) angle_deg -= 360.0F;
    }
};

Shaft g_shaft{};

sp01::ControllerSnapshot tick(sp01::Controller& ctl,
                              sp01::host::ManualClock& c,
                              sp01::host::VirtualIo& io,
                              sp01::host::VirtualWeigher& w,
                              std::uint64_t advance_us = 10000) {
    c.advance_us(advance_us);
    g_shaft.advance(advance_us);
    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest(),
                      g_shaft.snapshot());
    io.commit_outputs(s.outputs);
    return s;
}

// Turn the shaft until the controller leaves the given state, or give up.
sp01::ControllerSnapshot run_until_leaves(sp01::Controller& ctl,
                                          sp01::host::ManualClock& c,
                                          sp01::host::VirtualIo& io,
                                          sp01::host::VirtualWeigher& w,
                                          State state) {
    sp01::ControllerSnapshot s = ctl.snapshot();
    for (int i = 0; i < 4000 && s.state == state; ++i) {
        s = tick(ctl, c, io, w, 10000);
    }
    return s;
}

sp01::ControllerSnapshot enter_auto_coarse(sp01::Controller& ctl,
                                           sp01::host::ManualClock& c,
                                           sp01::host::VirtualIo& io,
                                           sp01::host::VirtualWeigher& w) {
    io.set_mode(OperationMode::Auto);
    set_auto_permissive(io);
    publish(w, c, 0.0F, true);

    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest(),
                      g_shaft.snapshot());
    REQUIRE(s.state == State::WaitFillPosition);

    io.set_input(Di::FillPosition, false);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::WaitFillPosition);

    io.set_input(Di::FillPosition, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::BagAcquire);

    io.set_input(Di::BagPresent, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::BagVerify);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::TareReady);

    publish(w, c, 0.0F, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::CoarseFill);
    return s;
}

sp01::ControllerSnapshot finish_to_auto_discharge(sp01::Controller& ctl,
                                                   sp01::host::ManualClock& c,
                                                   sp01::host::VirtualIo& io,
                                                   sp01::host::VirtualWeigher& w) {
    publish(w, c, 40.0F);
    auto s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::FineFill);

    publish(w, c, 50.0F);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Cutoff);
    REQUIRE(!sp01::output(s.outputs, Do::FillingMotor));
    REQUIRE(!sp01::output(s.outputs, Do::SpoutAeration));

    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Settle);

    publish(w, c, 50.0F, true);
    s = tick(ctl, c, io, w, 100000);
    REQUIRE(s.state == State::WaitDischarge);
    return s;
}

void normal_auto_cycle() {
    sp01::ControllerConfig cfg;
    cfg.settle_min_us = 100000;
    cfg.push_duration_us = 100000;

    g_shaft = Shaft{};
    g_shaft.angle_deg = 0.0F;

    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    auto s = enter_auto_coarse(ctl, c, io, w);
    REQUIRE(sp01::output(s.outputs, Do::DosingValveA));
    REQUIRE(sp01::output(s.outputs, Do::DosingValveB));
    REQUIRE(sp01::output(s.outputs, Do::DosingValveC));

    s = finish_to_auto_discharge(ctl, c, io, w);
    REQUIRE(s.state == State::WaitDischarge);
    REQUIRE(s.disposition == sp01::BagDisposition::Good);

    // Park the shaft at a known angle; the controller latches its reference on
    // the first tick after entering WaitDischarge.
    g_shaft.angle_deg = 0.0F;

    // A good bag must not be pushed at the reject angle.
    while (g_shaft.angle_deg < 250.0F && s.state == State::WaitDischarge) {
        s = tick(ctl, c, io, w, 10000);
    }
    REQUIRE(s.state == State::WaitDischarge);

    // It is pushed when the shaft reaches the normal discharge angle.
    s = run_until_leaves(ctl, c, io, w, State::WaitDischarge);
    REQUIRE(s.state == State::Push);
    REQUIRE(sp01::output(s.outputs, Do::BagPush));
    REQUIRE(g_shaft.angle_deg >= 353.0F);

    s = tick(ctl, c, io, w, 100000);
    REQUIRE(s.state == State::Complete);
    REQUIRE(sp01::all_outputs_off(s.outputs));
    REQUIRE(s.cycle_id == 1);
}

void discharge_requires_valid_position() {
    sp01::ControllerConfig cfg;
    cfg.settle_min_us = 100000;

    g_shaft = Shaft{};
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    auto s = finish_to_auto_discharge(ctl, c, io, w);
    REQUIRE(s.state == State::WaitDischarge);
    g_shaft.angle_deg = 0.0F;

    // Decoder loses sync: the controller must fault rather than guess.
    g_shaft.valid = false;
    s = tick(ctl, c, io, w, 10000);
    REQUIRE(s.state == State::Fault);
    REQUIRE(s.fault == Fault::DischargeTimingInvalid);
    REQUIRE(sp01::all_outputs_off(s.outputs));
}

void discharge_tracks_speed() {
    // Angle-based push holds at the same angle when the shaft runs slower.
    for (std::uint64_t rev : {14400000ULL, 21600000ULL}) {
        sp01::ControllerConfig cfg;
        cfg.settle_min_us = 100000;
        cfg.push_duration_us = 100000;
        cfg.wait_discharge_timeout_us = 30000000;

        g_shaft = Shaft{};
        g_shaft.revolution_us = rev;

        sp01::Controller ctl(cfg);
        sp01::host::ManualClock c;
        sp01::host::VirtualIo io;
        sp01::host::VirtualWeigher w;

        enter_auto_coarse(ctl, c, io, w);
        auto s = finish_to_auto_discharge(ctl, c, io, w);
        g_shaft.angle_deg = 0.0F;
        s = run_until_leaves(ctl, c, io, w, State::WaitDischarge);
        REQUIRE(s.state == State::Push);
        REQUIRE(g_shaft.angle_deg >= 353.0F);
        REQUIRE(g_shaft.angle_deg <= 359.9F);
    }
}

void manual_fill_without_rotation_or_push() {
    sp01::ControllerConfig cfg;
    cfg.settle_min_us = 100000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    io.set_mode(OperationMode::Manual);
    io.set_input(Di::HopperFeederRunning, true);
    io.set_input(Di::DownstreamConveyorReady, false);
    io.set_input(Di::MachineMotorRunning, false);
    io.set_input(Di::ProcessInitiative, false);
    publish(w, c, 0.0F, true);

    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest());
    REQUIRE(s.mode == OperationMode::Manual);
    REQUIRE(s.state == State::WaitPermissive);

    io.set_input(Di::ProcessInitiative, true);  // physical manual ON/OFF switch
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::BagAcquire);
    REQUIRE(sp01::output(s.outputs, Do::ScannerDown));
    REQUIRE(!sp01::output(s.outputs, Do::BagPush));

    io.set_input(Di::BagPresent, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::BagVerify);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::TareReady);

    publish(w, c, 0.0F, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::CoarseFill);
    REQUIRE(!sp01::output(s.outputs, Do::BagPush));

    publish(w, c, 40.0F);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::FineFill);

    publish(w, c, 50.0F);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Cutoff);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Settle);

    publish(w, c, 50.0F, true);
    s = tick(ctl, c, io, w, 100000);
    REQUIRE(s.state == State::Complete);
    REQUIRE(sp01::all_outputs_off(s.outputs));
    REQUIRE(!sp01::output(s.outputs, Do::BagPush));

    // ON stays latched complete: no automatic second fill.
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Complete);

    io.set_input(Di::ProcessInitiative, false);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::WaitPermissive);
}

void manual_off_stops_fill_cleanly() {
    sp01::Controller ctl;
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    io.set_mode(OperationMode::Manual);
    io.set_input(Di::HopperFeederRunning, true);
    io.set_input(Di::ProcessInitiative, true);
    publish(w, c, 0.0F, true);

    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest());
    REQUIRE(s.state == State::BagAcquire);
    io.set_input(Di::BagPresent, true);
    tick(ctl, c, io, w);
    tick(ctl, c, io, w);
    publish(w, c, 0.0F, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::CoarseFill);

    io.set_input(Di::ProcessInitiative, false);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::WaitPermissive);
    REQUIRE(s.fault == Fault::None);
    REQUIRE(sp01::all_outputs_off(s.outputs));
}

void mode_change_during_fill_faults_safe() {
    sp01::Controller ctl;
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    io.set_mode(OperationMode::Manual);
    const auto s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Fault);
    REQUIRE(s.fault == Fault::ModeChanged);
    REQUIRE(sp01::all_outputs_off(s.outputs));
}

}  // namespace

int main() {
    normal_auto_cycle();
    discharge_requires_valid_position();
    discharge_tracks_speed();
    manual_fill_without_rotation_or_push();
    manual_off_stops_fill_cleanly();
    mode_change_during_fill_faults_safe();
    return 0;
}
