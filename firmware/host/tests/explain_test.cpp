#include "sp01/controller_explain.hpp"
#include <cstdio>
#include <cstdlib>
using namespace sp01;
namespace {
int fails = 0;
void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++fails;
}
InputImage all_on() {
    InputImage in{};
    in.mode = OperationMode::Auto;
    for (auto& d : in.di) d = true;
    return in;
}
}  // namespace
int main() {
    ControllerConfig cfg{};
    ControllerSnapshot snap{};
    snap.mode = OperationMode::Auto;
    snap.state = State::WaitPermissive;
    WeightSnapshot w{};
    w.quality = WeightQuality::Good;
    w.sample_time_us = 1000;
    w.stable = true;
    PositionSnapshot p{};

    // Every permissive missing -> four separate reasons, not one string.
    InputImage none{};
    none.mode = OperationMode::Auto;
    auto e = explain_controller(snap, cfg, none, w, p, 1000);
    check(!e.permissive, "not permissive with all inputs off");
    check(e.blocked_by(Block::FeederNotRunning), "feeder listed");
    check(e.blocked_by(Block::DownstreamNotReady), "downstream listed");
    check(e.blocked_by(Block::MotorNotRunning), "motor listed");
    check(e.blocked_by(Block::NoInitiative), "initiative listed");

    // One missing -> exactly that one.
    InputImage one = all_on();
    one.di[static_cast<std::size_t>(Di::DownstreamConveyorReady)] = false;
    e = explain_controller(snap, cfg, one, w, p, 1000);
    check(e.blocked_by(Block::DownstreamNotReady), "only downstream listed");
    check(!e.blocked_by(Block::FeederNotRunning), "feeder not falsely listed");

    // Stale weight is a block.
    e = explain_controller(snap, cfg, all_on(), w, p, 1000 + cfg.weight_stale_us + 1);
    check(e.blocked_by(Block::WeightStale), "stale weight listed");

    // Waiting for the push angle, with distance to go.
    snap.state = State::WaitDischarge;
    snap.push_angle_deg = 355.0F;
    p.valid = true;
    p.angle_deg = 300.0F;
    e = explain_controller(snap, cfg, all_on(), w, p, 1000);
    check(e.next_state == State::Push, "next state is Push");
    check(e.blocked_by(Block::WaitingPushAngle), "waiting push angle listed");
    check(e.angle_to_push_deg > 54.0F && e.angle_to_push_deg < 56.0F,
          "55 deg remaining to push");

    // Distance wraps past 360.
    p.angle_deg = 10.0F;
    e = explain_controller(snap, cfg, all_on(), w, p, 1000);
    check(e.angle_to_push_deg > 344.0F && e.angle_to_push_deg < 346.0F,
          "wrap-safe distance to push");

    // Invalid position is its own reason.
    p.valid = false;
    e = explain_controller(snap, cfg, all_on(), w, p, 1000);
    check(e.blocked_by(Block::PositionInvalid), "invalid position listed");

    // Desired mask is read from the FSM outputs, not a local table.
    snap.state = State::CoarseFill;
    snap.outputs = OutputImage{};
    snap.outputs.channels[static_cast<std::size_t>(Do::DosingValveA)] = true;
    snap.outputs.channels[static_cast<std::size_t>(Do::FillingMotor)] = true;
    e = explain_controller(snap, cfg, all_on(), w, p, 1000);
    check(e.desired_mask == 0x48, "desired mask mirrors FSM outputs");

    // Every reason has text in both languages.
    for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(Block::Count); ++i) {
        const auto b = static_cast<Block>(i);
        if (block_text_en(b)[0] == '\0' || block_text_vi(b)[0] == '\0') {
            check(false, block_name(b));
        }
    }
    check(true, "all reasons have EN and VI text");

    std::printf("\n%s\n", fails == 0 ? "ALL PASS" : "FAILURES");
    return fails == 0 ? 0 : 1;
}
