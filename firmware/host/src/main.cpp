#include "sp01/host_sim.hpp"

#include <iostream>

int main() {
    sp01::host::ManualClock clock;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher weigher;

    io.commit_outputs(sp01::safe_output_image());
    weigher.publish(0.0F, true, sp01::WeightQuality::Good, clock.now_us());

    const bool safe = sp01::all_outputs_off(io.outputs());

    std::cout << "SP01 host simulator\n"
              << "DI=8 DO=8\n"
              << "weight=" << weigher.latest().net_kg << " kg\n"
              << "safe_outputs=" << (safe ? "OK" : "FAIL") << '\n';

    return safe ? 0 : 1;
}
