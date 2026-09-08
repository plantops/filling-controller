#include "sp01/controller.hpp"
#include "sp01/host_sim.hpp"

#include <cstdint>
#include <cstdlib>

#define REQUIRE(expr) do { if (!(expr)) std::abort(); } while (false)

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

sp01::ControllerSnapshot tick(sp01::Controller& ctl,
                              sp01::host::ManualClock& c,
                              sp01::host::VirtualIo& io,
                              sp01::host::VirtualWeigher& w,
                              std::uint64_t advance_us = 10000) {
    c.advance_us(advance_us);
    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest());
    io.commit_outputs(s.outputs);
    return s;
}

sp01::ControllerSnapshot enter_auto_coarse(sp01::Controller& ctl,
                                           sp01::host::ManualClock& c,
                                           sp01::host::VirtualIo& io,
                                           sp01::host::VirtualWeigher& w) {
    io.set_mode(OperationMode::Auto);
    set_auto_permissive(io);
    publish(w, c, 0.0F, true);

    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest());
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
    cfg.discharge_countdown_counts = 1000;
    cfg.discharge_lead_counts = 0;

    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    auto s = enter_auto_coarse(ctl, c, io, w);
    REQUIRE(sp01::output(s.outputs, Do::DosingValveA));
    REQUIRE(sp01::output(s.outputs, Do::DosingValveB));
    REQUIRE(sp01::output(s.outputs, Do::DosingValveC));

    s = finish_to_auto_discharge(ctl, c, io, w);

    io.set_input(Di::DischargeRefA, true);
    s = tick(ctl, c, io, w, 10000);
    REQUIRE(s.state == State::WaitDischarge);

    io.set_input(Di::DischargeRefA, false);
    tick(ctl, c, io, w, 10000);

    io.set_input(Di::DischargeRefB, true);
    s = tick(ctl, c, io, w, 90000);
    REQUIRE(s.discharge_ref_interval_us == 100000);
    REQUIRE(s.discharge_due_us == c.now_us() + 100000);
    REQUIRE(s.state == State::WaitDischarge);

    io.set_input(Di::DischargeRefB, false);
    s = tick(ctl, c, io, w, 100000);
    REQUIRE(s.state == State::Push);
    REQUIRE(sp01::output(s.outputs, Do::BagPush));

    s = tick(ctl, c, io, w, 100000);
    REQUIRE(s.state == State::Complete);
    REQUIRE(sp01::all_outputs_off(s.outputs));
    REQUIRE(s.cycle_id == 1);
}

std::uint64_t measured_countdown(std::uint64_t ab_interval_us) {
    sp01::ControllerConfig cfg;
    cfg.settle_min_us = 100000;
    cfg.discharge_countdown_counts = 1000;
    cfg.discharge_lead_counts = 100;

    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    finish_to_auto_discharge(ctl, c, io, w);

    io.set_input(Di::DischargeRefA, true);
    auto s = tick(ctl, c, io, w, 10000);
    REQUIRE(s.state == State::WaitDischarge);
    const auto a_time = c.now_us();

    io.set_input(Di::DischargeRefA, false);
    tick(ctl, c, io, w, 10000);

    io.set_input(Di::DischargeRefB, true);
    const auto remaining = ab_interval_us - 10000;
    s = tick(ctl, c, io, w, remaining);
    REQUIRE(s.discharge_ref_interval_us == ab_interval_us);
    REQUIRE(c.now_us() - a_time == ab_interval_us);
    return s.discharge_due_us - c.now_us();
}

void discharge_tracks_speed() {
    REQUIRE(measured_countdown(100000) == 90000);
    REQUIRE(measured_countdown(50000) == 45000);
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
    discharge_tracks_speed();
    manual_fill_without_rotation_or_push();
    manual_off_stops_fill_cleanly();
    mode_change_during_fill_faults_safe();
    return 0;
}
