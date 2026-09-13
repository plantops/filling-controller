#include "sp01/controller.hpp"
#include "sp01/host_sim.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Simulated shaft shared by the helpers below. The controller now decides push
// points by angle, so every test must turn the shaft.
struct Shaft {
    bool valid{true};
    std::uint64_t revolution_us{14400000};
    float angle_deg{0.0F};
    sp01::PositionSnapshot snapshot() const {
        sp01::PositionSnapshot p{};
        p.valid = valid; p.angle_deg = angle_deg; p.revolution_us = revolution_us;
        return p;
    }
    void advance(std::uint64_t us) {
        angle_deg += 360.0F * static_cast<float>(us) /
                     static_cast<float>(revolution_us);
        while (angle_deg >= 360.0F) angle_deg -= 360.0F;
    }
};
Shaft g_shaft{};

#define REQUIRE(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); std::abort(); } } while (false)

namespace {
using sp01::BagDisposition;
using sp01::Di;
using sp01::Do;
using sp01::OperationMode;
using sp01::PositionSnapshot;
using sp01::State;
using sp01::WeightQuality;

void set_auto_permissive(sp01::host::VirtualIo& io) {
    io.set_input(Di::HopperFeederRunning, true);
    io.set_input(Di::DownstreamConveyorReady, true);
    io.set_input(Di::MachineMotorRunning, true);
    io.set_input(Di::ProcessInitiative, true);
}

void publish(sp01::host::VirtualWeigher& w,
             const sp01::host::ManualClock& c,
             float kg,
             bool stable = false) {
    w.publish(kg, stable, WeightQuality::Good, c.now_us());
}

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

sp01::ControllerSnapshot enter_auto_coarse(sp01::Controller& ctl,
                                           sp01::host::ManualClock& c,
                                           sp01::host::VirtualIo& io,
                                           sp01::host::VirtualWeigher& w) {
    io.set_mode(OperationMode::Auto);
    set_auto_permissive(io);
    publish(w, c, 0.0F, true);

    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest(), g_shaft.snapshot());
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

void require_fill_outputs_on(const sp01::ControllerSnapshot& s) {
    REQUIRE(sp01::output(s.outputs, Do::DosingValveA));
    REQUIRE(sp01::output(s.outputs, Do::DosingValveB) || s.state == State::FineFill);
    REQUIRE(sp01::output(s.outputs, Do::DosingValveC));
    REQUIRE(sp01::output(s.outputs, Do::FillingMotor));
    REQUIRE(sp01::output(s.outputs, Do::SpoutAeration));
}

void require_fill_outputs_off(const sp01::ControllerSnapshot& s) {
    REQUIRE(!sp01::output(s.outputs, Do::DosingValveA));
    REQUIRE(!sp01::output(s.outputs, Do::DosingValveB));
    REQUIRE(!sp01::output(s.outputs, Do::DosingValveC));
    REQUIRE(!sp01::output(s.outputs, Do::FillingMotor));
    REQUIRE(!sp01::output(s.outputs, Do::SpoutAeration));
}

sp01::ControllerConfig reject_config() {
    sp01::ControllerConfig cfg;
    cfg.broken_bag_loss_trip_kg = 0.5F;   // test-only; production frozen in G4/G8
    cfg.broken_bag_persist_us = 20000;    // test-only
    cfg.reject_wait_timeout_us = 20000000;  // > one revolution  // test-only
    cfg.push_duration_us = 50000;
    cfg.weight_stale_us = 500000;
    return cfg;
}

void increasing_weight_remains_good_candidate() {
    auto cfg = reject_config();
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    auto s = enter_auto_coarse(ctl, c, io, w);
    for (float kg : {5.0F, 10.0F, 15.0F, 20.0F, 25.0F}) {
        publish(w, c, kg);
        s = tick(ctl, c, io, w, 20000);
        REQUIRE(s.state == State::CoarseFill);
        REQUIRE(s.disposition == BagDisposition::Undecided);
        require_fill_outputs_on(s);
    }
}

void single_negative_spike_does_not_reject() {
    auto cfg = reject_config();
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    auto s = enter_auto_coarse(ctl, c, io, w);
    publish(w, c, 10.0F);
    s = tick(ctl, c, io, w, 20000);
    publish(w, c, 9.0F);
    s = tick(ctl, c, io, w, 10000);
    REQUIRE(s.state == State::CoarseFill);
    publish(w, c, 10.2F);
    s = tick(ctl, c, io, w, 10000);
    REQUIRE(s.state == State::CoarseFill);
    REQUIRE(s.disposition == BagDisposition::Undecided);
    require_fill_outputs_on(s);
}

void sustained_loss_immediately_stops_fill_and_latches_reject() {
    auto cfg = reject_config();
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    auto s = enter_auto_coarse(ctl, c, io, w);
    publish(w, c, 12.0F);
    s = tick(ctl, c, io, w, 20000);
    REQUIRE(s.state == State::CoarseFill);

    publish(w, c, 11.0F);
    s = tick(ctl, c, io, w, 10000);
    REQUIRE(s.state == State::CoarseFill);

    publish(w, c, 10.8F);
    s = tick(ctl, c, io, w, 20000);
    REQUIRE(s.state == State::RejectWait);
    REQUIRE(s.disposition == BagDisposition::Reject);
    REQUIRE(s.broken_bag_detected_us == c.now_us());
    REQUIRE(s.broken_bag_peak_kg >= 12.0F);
    require_fill_outputs_off(s);
    REQUIRE(!sp01::output(s.outputs, Do::BagPush));
}

void reject_pushes_at_210_and_never_uses_normal_path() {
    auto cfg = reject_config();
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    publish(w, c, 12.0F);
    tick(ctl, c, io, w, 20000);
    publish(w, c, 11.0F);
    tick(ctl, c, io, w, 10000);
    publish(w, c, 10.7F);
    auto s = tick(ctl, c, io, w, 20000);
    REQUIRE(s.state == State::RejectWait);

    // Turn the shaft to the reject angle; the controller pushes there.
    g_shaft.angle_deg = 190.0F;
    for (int i = 0; i < 400 && s.state == State::RejectWait; ++i) {
        s = tick(ctl, c, io, w, 10000);
    }
    REQUIRE(s.state == State::Push);
    REQUIRE(s.disposition == BagDisposition::Reject);
    REQUIRE(sp01::output(s.outputs, Do::BagPush));
    require_fill_outputs_off(s);

    s = tick(ctl, c, io, w, cfg.push_duration_us);
    REQUIRE(s.state == State::Complete);
    REQUIRE(s.disposition == BagDisposition::Reject);
    REQUIRE(sp01::all_outputs_off(s.outputs));

    // A completed reject cycle cannot re-enter the normal-discharge path.
    io.set_input(Di::PositionIndex, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::WaitFillPosition);
    REQUIRE(!sp01::output(s.outputs, Do::BagPush));
}

void good_path_ignores_210_and_uses_normal_discharge() {
    auto cfg = reject_config();
    cfg.settle_min_us = 20000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    publish(w, c, 40.0F);
    auto s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::FineFill);
    publish(w, c, 50.0F);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Cutoff);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::Settle);
    publish(w, c, 50.0F, true);
    s = tick(ctl, c, io, w, cfg.settle_min_us);
    REQUIRE(s.state == State::WaitDischarge);
    REQUIRE(s.disposition == BagDisposition::Good);

    g_shaft.angle_deg = 190.0F;
    for (int i = 0; i < 400 && s.state == State::RejectWait; ++i) {
        s = tick(ctl, c, io, w);
    }
    REQUIRE(s.state == State::WaitDischarge);
    REQUIRE(!sp01::output(s.outputs, Do::BagPush));

    // Carrying on round, the good bag is pushed at the normal discharge angle.
    for (int i = 0; i < 1000 && s.state == State::WaitDischarge; ++i) {
        s = tick(ctl, c, io, w);
    }
    REQUIRE(s.state == State::Push);
    REQUIRE(s.disposition == BagDisposition::Good);
    REQUIRE(sp01::output(s.outputs, Do::BagPush));
}

void reject_window_timeout_faults_safe() {
    auto cfg = reject_config();
    cfg.reject_wait_timeout_us = 20000000;
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    enter_auto_coarse(ctl, c, io, w);
    publish(w, c, 10.0F);
    tick(ctl, c, io, w, 20000);
    publish(w, c, 9.0F);
    tick(ctl, c, io, w, 10000);
    publish(w, c, 8.8F);
    auto s = tick(ctl, c, io, w, 20000);
    REQUIRE(s.state == State::RejectWait);

    s = tick(ctl, c, io, w, cfg.reject_wait_timeout_us);
    REQUIRE(s.state == State::Fault);
    REQUIRE(sp01::all_outputs_off(s.outputs));
}

void detector_is_disabled_outside_fill() {
    auto cfg = reject_config();
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    io.set_mode(OperationMode::Auto);
    set_auto_permissive(io);
    publish(w, c, 10.0F, true);
    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest(), g_shaft.snapshot());
    REQUIRE(s.state == State::WaitFillPosition);

    publish(w, c, 0.0F, true);
    s = tick(ctl, c, io, w, 50000);
    REQUIRE(s.state == State::WaitFillPosition);
    REQUIRE(s.disposition == BagDisposition::Undecided);
}

void manual_broken_bag_stops_without_automatic_push() {
    auto cfg = reject_config();
    sp01::Controller ctl(cfg);
    sp01::host::ManualClock c;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher w;

    io.set_mode(OperationMode::Manual);
    io.set_input(Di::HopperFeederRunning, true);
    io.set_input(Di::ProcessInitiative, true);
    publish(w, c, 0.0F, true);
    auto s = ctl.tick(c.now_us(), io.read_inputs(), w.latest(), g_shaft.snapshot());
    REQUIRE(s.state == State::BagAcquire);
    io.set_input(Di::BagPresent, true);
    tick(ctl, c, io, w);
    tick(ctl, c, io, w);
    publish(w, c, 0.0F, true);
    s = tick(ctl, c, io, w);
    REQUIRE(s.state == State::CoarseFill);

    publish(w, c, 10.0F);
    tick(ctl, c, io, w, 20000);
    publish(w, c, 9.0F);
    tick(ctl, c, io, w, 10000);
    publish(w, c, 8.7F);
    s = tick(ctl, c, io, w, 20000);
    REQUIRE(s.state == State::Complete);
    REQUIRE(s.disposition == BagDisposition::Reject);
    REQUIRE(!sp01::output(s.outputs, Do::BagPush));
    require_fill_outputs_off(s);
}

}  // namespace

int main() {
    increasing_weight_remains_good_candidate();
    single_negative_spike_does_not_reject();
    sustained_loss_immediately_stops_fill_and_latches_reject();
    reject_pushes_at_210_and_never_uses_normal_path();
    good_path_ignores_210_and_uses_normal_discharge();
    reject_window_timeout_faults_safe();
    detector_is_disabled_outside_fill();
    manual_broken_bag_stops_without_automatic_push();
    return 0;
}
