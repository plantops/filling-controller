#include "sp01/controller.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); \
        std::abort(); \
    } \
} while (false)

namespace {

using namespace sp01;

struct Rig {
    InputImage in{};
    WeightSnapshot weight{};
    std::uint64_t now_us{0};

    void set(Di channel, bool value) {
        in.di[static_cast<std::size_t>(channel)] = value;
    }

    void set_weight(float kg,
                    bool stable = false,
                    WeightQuality quality = WeightQuality::Good) {
        weight.net_kg = kg;
        weight.stable = stable;
        weight.quality = quality;
        weight.sample_time_us = now_us;
        ++weight.sequence;
    }
};

void set_permissive(Rig& rig, bool value = true) {
    rig.set(Di::HopperFeederRunning, value);
    rig.set(Di::DownstreamConveyorReady, value);
    rig.set(Di::MachineMotorRunning, value);
    rig.set(Di::ProcessInitiative, value);
}

ControllerSnapshot tick(Controller& controller,
                        Rig& rig,
                        std::uint64_t advance_us = 10000) {
    rig.now_us += advance_us;
    if (rig.weight.quality == WeightQuality::Good) {
        rig.weight.sample_time_us = rig.now_us;
    }
    return controller.tick(rig.now_us, rig.in, rig.weight);
}

void advance_to_wait_discharge(Controller& controller, Rig& rig) {
    set_permissive(rig);
    rig.set_weight(0.0F, true);

    auto snapshot = controller.tick(rig.now_us, rig.in, rig.weight);
    REQUIRE(snapshot.state == State::WaitFillPosition);

    rig.set(Di::FillPosition, true);
    snapshot = tick(controller, rig);
    REQUIRE(snapshot.state == State::BagAcquire);

    rig.set(Di::BagPresent, true);
    snapshot = tick(controller, rig);
    REQUIRE(snapshot.state == State::BagVerify);
    snapshot = tick(controller, rig);
    REQUIRE(snapshot.state == State::TareReady);
    snapshot = tick(controller, rig);
    REQUIRE(snapshot.state == State::CoarseFill);

    rig.set_weight(40.0F);
    snapshot = tick(controller, rig);
    REQUIRE(snapshot.state == State::FineFill);

    rig.set_weight(50.0F);
    snapshot = tick(controller, rig);
    REQUIRE(snapshot.state == State::Cutoff);
    snapshot = tick(controller, rig);
    REQUIRE(snapshot.state == State::Settle);

    rig.set_weight(50.0F, true);
    snapshot = tick(controller, rig, 100000);
    REQUIRE(snapshot.state == State::WaitDischarge);
}

void speed_adaptive_discharge(std::uint64_t a_to_b_us,
                              std::uint64_t expected_delay_after_b_us) {
    ControllerConfig config;
    config.settle_min_us = 100000;
    config.push_duration_us = 100000;
    config.discharge_ref_span_deg = 10.0F;
    config.discharge_target_after_b_deg = 5.0F;
    config.discharge_actuator_delay_us = 10000;

    Controller controller(config);
    Rig rig;
    advance_to_wait_discharge(controller, rig);

    rig.set(Di::DischargeRefA, false);
    rig.set(Di::DischargeRefB, false);
    tick(controller, rig);

    rig.set(Di::DischargeRefA, true);
    auto snapshot = tick(controller, rig);
    REQUIRE(snapshot.state == State::WaitDischarge);

    rig.set(Di::DischargeRefA, false);
    if (a_to_b_us > 10000) {
        tick(controller, rig, a_to_b_us - 10000);
    }

    rig.set(Di::DischargeRefB, true);
    snapshot = tick(controller, rig, 10000);

    REQUIRE(snapshot.state == State::WaitDischarge);
    REQUIRE(snapshot.discharge_ref_interval_us == a_to_b_us);
    REQUIRE(snapshot.discharge_command_due_us - rig.now_us ==
            expected_delay_after_b_us);

    if (expected_delay_after_b_us > 0) {
        snapshot = tick(controller, rig, expected_delay_after_b_us - 1);
        REQUIRE(snapshot.state == State::WaitDischarge);
        snapshot = tick(controller, rig, 1);
    }

    REQUIRE(snapshot.state == State::Push);
    REQUIRE(output(snapshot.outputs, Do::BagPush));
}

void invalid_geometry_rejected() {
    ControllerConfig config;
    config.settle_min_us = 100000;
    config.discharge_ref_span_deg = 15.0F;
    config.discharge_target_after_b_deg = 0.0F;
    config.discharge_actuator_delay_us = 10000;

    Controller controller(config);
    Rig rig;
    advance_to_wait_discharge(controller, rig);

    rig.set(Di::DischargeRefA, false);
    rig.set(Di::DischargeRefB, false);
    tick(controller, rig);
    rig.set(Di::DischargeRefA, true);
    tick(controller, rig);
    rig.set(Di::DischargeRefA, false);
    tick(controller, rig, 80000);
    rig.set(Di::DischargeRefB, true);

    const auto snapshot = tick(controller, rig, 10000);
    REQUIRE(snapshot.state == State::Fault);
    REQUIRE(snapshot.fault == Fault::DischargeTimingInvalid);
    REQUIRE(all_outputs_off(snapshot.outputs));
}

void stale_weight_fault() {
    ControllerConfig config;
    config.weight_stale_us = 100000;
    Controller controller(config);
    Rig rig;

    set_permissive(rig);
    rig.set_weight(0.0F, true);
    auto snapshot = controller.tick(rig.now_us, rig.in, rig.weight);
    REQUIRE(snapshot.state == State::WaitFillPosition);

    rig.set(Di::FillPosition, true);
    tick(controller, rig);
    rig.set(Di::BagPresent, true);
    tick(controller, rig);
    tick(controller, rig);

    rig.now_us += 200000;
    snapshot = controller.tick(rig.now_us, rig.in, rig.weight);
    REQUIRE(snapshot.state == State::Fault);
    REQUIRE(snapshot.fault == Fault::WeightStale);
    REQUIRE(all_outputs_off(snapshot.outputs));
}

void permissive_loss_fault() {
    ControllerConfig config;
    config.discharge_ref_span_deg = 10.0F;
    config.discharge_target_after_b_deg = 5.0F;
    Controller controller(config);
    Rig rig;

    set_permissive(rig);
    rig.set_weight(0.0F, true);
    const auto first = controller.tick(rig.now_us, rig.in, rig.weight);
    REQUIRE(first.state == State::WaitFillPosition);

    rig.set(Di::FillPosition, true);
    tick(controller, rig);
    rig.set(Di::BagPresent, true);
    tick(controller, rig);
    tick(controller, rig);

    auto snapshot = tick(controller, rig);
    REQUIRE(snapshot.state == State::CoarseFill);

    rig.set(Di::ProcessInitiative, false);
    rig.set_weight(5.0F);
    snapshot = tick(controller, rig);
    REQUIRE(snapshot.state == State::Fault);
    REQUIRE(snapshot.fault == Fault::PermissiveLost);
    REQUIRE(all_outputs_off(snapshot.outputs));
}

}  // namespace

int main() {
    speed_adaptive_discharge(100000, 40000);
    speed_adaptive_discharge(50000, 15000);
    invalid_geometry_rejected();
    stale_weight_fault();
    permissive_loss_fault();
    return 0;
}
