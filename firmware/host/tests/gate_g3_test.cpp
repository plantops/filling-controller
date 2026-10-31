#include "sp01/controller.hpp"
#include "sp01/host_sim.hpp"

#include <cstdint>
#include <cstdlib>

#define REQUIRE(expr) do { if (!(expr)) std::abort(); } while (false)

namespace {

using sp01::Di;
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

void require_fault_safe(const sp01::ControllerSnapshot& s, Fault fault) {
    REQUIRE(s.state == State::Fault);
    REQUIRE(s.fault == fault);
    REQUIRE(sp01::all_outputs_off(s.outputs));
}

sp01::ControllerSnapshot enter_auto_bag_acquire(sp01::Controller& ctl,
                                                sp01::host::ManualClock& c,
                                                sp01::host::VirtualIo& io,
                                                sp01::host::VirtualWeigher& w) {
    io.set_mode(OperationMode::Auto);
    set_auto_permissive(io);
    publish(w, c, 0.0F, true);

    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest());
    REQUIRE(s.state == State::WaitFillPosition);

    io.set_input(Di::FillPosition, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::BagAcquire);
    return s;
}

sp01::ControllerSnapshot enter_auto_tare_ready(sp01::Controller& ctl,
                                               sp01::host::ManualClock& c,
                                               sp01::host::VirtualIo& io,
                                               sp01::host::VirtualWeigher& w) {
    auto s = enter_auto_bag_acquire(ctl, c, io, w);
    io.set_input(Di::BagPresent, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::BagVerify);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::TareReady);
    return s;
}

sp01::ControllerSnapshot enter_auto_coarse(sp01::Controller& ctl,
                                           sp01::host::ManualClock& c,
                                           sp01::host::VirtualIo& io,
                                           sp01::host::VirtualWeigher& w) {
    auto s = enter_auto_tare_ready(ctl, c, io, w);
    publish(w, c, 0.0F, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::CoarseFill);
    return s;
}

sp01::ControllerSnapshot enter_auto_fine(sp01::Controller& ctl,
                                         sp01::host::ManualClock& c,
                                         sp01::host::VirtualIo& io,
                                         sp01::host::VirtualWeigher& w) {
    enter_auto_coarse(ctl, c, io, w);
    publish(w, c, 40.0F);
    const auto s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::FineFill);
    return s;
}

sp01::ControllerSnapshot enter_auto_settle(sp01::Controller& ctl,
                                           sp01::host::ManualClock& c,
                                           sp01::host::VirtualIo& io,
                                           sp01::host::VirtualWeigher& w) {
    enter_auto_fine(ctl, c, io, w);
    publish(w, c, 50.0F);
    auto s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Cutoff);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Settle);
    return s;
}

sp01::ControllerSnapshot enter_auto_wait_discharge(sp01::Controller& ctl,
                                                   sp01::host::ManualClock& c,
                                                   sp01::host::VirtualIo& io,
                                                   sp01::host::VirtualWeigher& w) {
    enter_auto_settle(ctl, c, io, w);
    publish(w, c, 50.0F, true);
    const auto s = tick(ctl, c, io, w, ctl.config().settle_min_us);
    REQUIRE(s.state == State::WaitDischarge);
    return s;
}

void bag_acquire_timeout_faults_safe() {
    sp01::ControllerConfig cfg;
    cfg.bag_acquire_timeout_us = 100000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_bag_acquire(ctl, c, io, w);
    const auto s = tick(ctl, c, io, w, cfg.bag_acquire_timeout_us);
    require_fault_safe(s, Fault::BagMissing);
}

void permissive_loss_faults_safe() {
    sp01::Controller ctl;
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    io.set_input(Di::DownstreamConveyorReady, false);
    const auto s = tick(ctl, c, io, w);
    require_fault_safe(s, Fault::PermissiveLost);
}

void bag_lost_faults_safe() {
    sp01::Controller ctl;
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    io.set_input(Di::BagPresent, false);
    const auto s = tick(ctl, c, io, w);
    require_fault_safe(s, Fault::BagLost);
}

void stale_weight_at_tare_faults_safe() {
    sp01::ControllerConfig cfg;
    cfg.weight_stale_us = 10000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_tare_ready(ctl, c, io, w);
    const auto s = tick(ctl, c, io, w, 20000);
    require_fault_safe(s, Fault::WeightStale);
}

void stale_weight_at_coarse_faults_safe() {
    sp01::ControllerConfig cfg;
    cfg.weight_stale_us = 50000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    const auto s = tick(ctl, c, io, w, 50000);
    require_fault_safe(s, Fault::WeightStale);
}

void stale_weight_at_fine_faults_safe() {
    sp01::ControllerConfig cfg;
    cfg.weight_stale_us = 50000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_fine(ctl, c, io, w);
    const auto s = tick(ctl, c, io, w, 50000);
    require_fault_safe(s, Fault::WeightStale);
}

void stale_weight_at_settle_faults_safe() {
    sp01::ControllerConfig cfg;
    cfg.weight_stale_us = 50000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_settle(ctl, c, io, w);
    const auto s = tick(ctl, c, io, w, 50000);
    require_fault_safe(s, Fault::WeightStale);
}

void transmitter_fault_during_fill_faults_safe() {
    sp01::Controller ctl;
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    publish(w, c, 0.0F, false, WeightQuality::Fault);
    const auto s = tick(ctl, c, io, w);
    require_fault_safe(s, Fault::WeightFault);
}

void coarse_timeout_faults_safe() {
    sp01::ControllerConfig cfg;
    cfg.coarse_timeout_us = 100000;
    cfg.weight_stale_us = 500000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    publish(w, c, 5.0F);
    const auto s = tick(ctl, c, io, w, cfg.coarse_timeout_us);
    require_fault_safe(s, Fault::StateTimeout);
}

void fine_timeout_faults_safe() {
    sp01::ControllerConfig cfg;
    cfg.fine_timeout_us = 100000;
    cfg.weight_stale_us = 500000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_fine(ctl, c, io, w);
    publish(w, c, 45.0F);
    const auto s = tick(ctl, c, io, w, cfg.fine_timeout_us);
    require_fault_safe(s, Fault::StateTimeout);
}

void discharge_timeout_faults_safe() {
    sp01::ControllerConfig cfg;
    cfg.settle_min_us = 100000;
    cfg.wait_discharge_timeout_us = 100000;
    cfg.weight_stale_us = 1000000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_wait_discharge(ctl, c, io, w);
    const auto s = tick(ctl, c, io, w, cfg.wait_discharge_timeout_us);
    require_fault_safe(s, Fault::StateTimeout);
}

void b_before_a_faults_safe() {
    sp01::ControllerConfig cfg;
    cfg.settle_min_us = 100000;
    cfg.weight_stale_us = 1000000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_wait_discharge(ctl, c, io, w);
    io.set_input(Di::DischargeRefB, true);
    const auto s = tick(ctl, c, io, w);
    require_fault_safe(s, Fault::DischargeTimingInvalid);
}

void forced_io_fault_and_reset_are_safe() {
    sp01::Controller ctl;
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    ctl.force_fault(Fault::IoFault, c.now_us());
    require_fault_safe(ctl.snapshot(), Fault::IoFault);

    ctl.reset(c.now_us());
    REQUIRE(ctl.snapshot().state == State::WaitPermissive);
    REQUIRE(ctl.snapshot().fault == Fault::None);
    REQUIRE(sp01::all_outputs_off(ctl.snapshot().outputs));
}

void fault_clear_requires_initiative_off() {
    sp01::Controller ctl;
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    ctl.force_fault(Fault::IoFault, c.now_us());
    require_fault_safe(ctl.snapshot(), Fault::IoFault);

    REQUIRE(!ctl.clear_fault(c.now_us(), io.read_inputs()));
    REQUIRE(ctl.snapshot().state == State::Fault);

    io.set_input(Di::ProcessInitiative, false);
    REQUIRE(ctl.clear_fault(c.now_us(), io.read_inputs()));
    REQUIRE(ctl.snapshot().state == State::WaitPermissive);
    REQUIRE(ctl.snapshot().fault == Fault::None);
    REQUIRE(sp01::all_outputs_off(ctl.snapshot().outputs));
}

}  // namespace

int main() {
    bag_acquire_timeout_faults_safe();
    permissive_loss_faults_safe();
    bag_lost_faults_safe();
    stale_weight_at_tare_faults_safe();
    stale_weight_at_coarse_faults_safe();
    stale_weight_at_fine_faults_safe();
    stale_weight_at_settle_faults_safe();
    transmitter_fault_during_fill_faults_safe();
    coarse_timeout_faults_safe();
    fine_timeout_faults_safe();
    discharge_timeout_faults_safe();
    b_before_a_faults_safe();
    forced_io_fault_and_reset_are_safe();
    fault_clear_requires_initiative_off();
    return 0;
}
