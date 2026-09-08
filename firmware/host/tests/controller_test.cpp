#include "sp01/controller.hpp"
#include "sp01/host_sim.hpp"

#include <cstdlib>
#include <cstdint>

#define REQUIRE(expr) do { if (!(expr)) std::abort(); } while (false)

namespace {

using sp01::Di;
using sp01::Do;
using sp01::Fault;
using sp01::State;
using sp01::WeightQuality;

void set_permissive(sp01::host::VirtualIo& io, bool value = true) {
    io.set_input(Di::HopperFeederRunning, value);
    io.set_input(Di::DownstreamConveyorReady, value);
    io.set_input(Di::MachineMotorRunning, value);
    io.set_input(Di::ProcessInitiative, value);
}

void publish(sp01::host::VirtualWeigher& w,
             const sp01::host::ManualClock& c,
             float kg,
             bool stable = false,
             WeightQuality q = WeightQuality::Good) {
    w.publish(kg, stable, q, c.now_us());
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

void normal_cycle() {
    sp01::ControllerConfig cfg;
    cfg.settle_min_us = 100000;
    cfg.push_duration_us = 100000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    set_permissive(io);
    publish(w, c, 0.0F, true);

    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest());
    REQUIRE(s.state == State::WaitFillPosition);

    io.set_input(Di::FillPosition, false);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::WaitFillPosition);

    io.set_input(Di::FillPosition, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::BagAcquire);
    REQUIRE(sp01::output(s.outputs, Do::ScannerDown));
    REQUIRE(sp01::output(s.outputs, Do::BagDetectAir));

    io.set_input(Di::BagPresent, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::BagVerify);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::TareReady);

    publish(w, c, 0.0F, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::CoarseFill);
    REQUIRE(sp01::output(s.outputs, Do::DosingValveA));
    REQUIRE(sp01::output(s.outputs, Do::DosingValveB));
    REQUIRE(sp01::output(s.outputs, Do::DosingValveC));

    publish(w, c, 40.0F);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::FineFill);
    REQUIRE(sp01::output(s.outputs, Do::DosingValveA));
    REQUIRE(!sp01::output(s.outputs, Do::DosingValveB));
    REQUIRE(sp01::output(s.outputs, Do::DosingValveC));

    publish(w, c, 50.0F);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Cutoff);
    REQUIRE(!sp01::output(s.outputs, Do::FillingMotor));
    REQUIRE(!sp01::output(s.outputs, Do::SpoutAeration));

    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Settle);
    publish(w, c, 50.0F, true);
    s = tick(ctl, c, io, w, 100000);
    REQUIRE(s.state == State::WaitPushPosition);

    io.set_input(Di::PushPosition, false);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::WaitPushPosition);
    io.set_input(Di::PushPosition, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Push);
    REQUIRE(sp01::output(s.outputs, Do::BagPush));
    REQUIRE(!sp01::output(s.outputs, Do::ScannerDown));

    s = tick(ctl, c, io, w, 100000);
    REQUIRE(s.state == State::Complete);
    REQUIRE(sp01::all_outputs_off(s.outputs));
    s = tick(ctl, c, io, w);
    REQUIRE(s.cycle_id == 1);
}

void stale_weight_fault() {
    sp01::ControllerConfig cfg;
    cfg.weight_stale_us = 100000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;
    set_permissive(io);
    publish(w, c, 0.0F, true);
    auto first = ctl.tick(c.now_us(), io.read_inputs(), w.latest());
    REQUIRE(first.state == State::WaitFillPosition);
    io.set_input(Di::FillPosition, false);
    tick(ctl, c, io, w);
    io.set_input(Di::FillPosition, true);
    tick(ctl, c, io, w);
    io.set_input(Di::BagPresent, true);
    tick(ctl, c, io, w);
    tick(ctl, c, io, w);
    c.advance_us(200000);
    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest());
    REQUIRE(s.state == State::Fault);
    REQUIRE(s.fault == Fault::WeightStale);
    REQUIRE(sp01::all_outputs_off(s.outputs));
}

void permissive_loss_fault() {
    sp01::Controller ctl;
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;
    set_permissive(io);
    publish(w, c, 0.0F, true);
    auto first2 = ctl.tick(c.now_us(), io.read_inputs(), w.latest());
    REQUIRE(first2.state == State::WaitFillPosition);
    io.set_input(Di::FillPosition, false);
    tick(ctl, c, io, w);
    io.set_input(Di::FillPosition, true);
    tick(ctl, c, io, w);
    io.set_input(Di::BagPresent, true);
    tick(ctl, c, io, w);
    tick(ctl, c, io, w);
    publish(w, c, 0.0F, true);
    auto s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::CoarseFill);
    io.set_input(Di::ProcessInitiative, false);
    publish(w, c, 5.0F);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Fault);
    REQUIRE(s.fault == Fault::PermissiveLost);
    REQUIRE(sp01::all_outputs_off(s.outputs));
}

}  // namespace

int main() {
    normal_cycle();
    stale_weight_fault();
    permissive_loss_fault();
    return 0;
}
