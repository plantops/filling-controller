#include "sp01/controller.hpp"
#include "sp01/host_sim.hpp"

#include <iostream>

int main() {
    sp01::host::ManualClock clock;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher weigher;
    sp01::Controller controller;

    io.set_input(sp01::Di::HopperFeederRunning, true);
    io.set_input(sp01::Di::DownstreamConveyorReady, true);
    io.set_input(sp01::Di::MachineMotorRunning, true);
    io.set_input(sp01::Di::ProcessInitiative, true);
    weigher.publish(0.0F, true, sp01::WeightQuality::Good, clock.now_us());

    auto s = controller.tick(clock.now_us(), io.read_inputs(), weigher.latest());
    std::cout << "SP01 host: state=" << sp01::state_name(s.state)
              << " safe=" << (sp01::all_outputs_off(s.outputs) ? "yes" : "no") << '\n';
    return 0;
}
