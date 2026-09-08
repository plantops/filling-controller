#include "sp01/host_sim.hpp"

#include <cassert>
#include <cstddef>

int main() {
    sp01::host::ManualClock clock;
    sp01::host::VirtualIo io;
    sp01::host::VirtualWeigher weigher;

    assert(sp01::all_outputs_off(io.outputs()));

    io.set_input(sp01::Di::BagPresent, true);
    assert(io.input(sp01::Di::BagPresent));

    auto outputs = sp01::safe_output_image();
    outputs.channels[static_cast<std::size_t>(sp01::Do::ScannerDown)] = true;
    io.commit_outputs(outputs);
    assert(io.output(sp01::Do::ScannerDown));

    io.commit_outputs(sp01::safe_output_image());
    assert(sp01::all_outputs_off(io.outputs()));

    weigher.publish(20.0F, true, sp01::WeightQuality::Good, clock.now_us());
    assert(weigher.latest().sequence == 1U);
    assert(weigher.latest().net_kg == 20.0F);
    assert(weigher.latest().stable);

    clock.advance_us(1000U);
    weigher.publish(50.0F, true, sp01::WeightQuality::Good, clock.now_us());
    assert(weigher.latest().sequence == 2U);
    assert(weigher.latest().sample_time_us == 1000U);
    assert(weigher.latest().net_kg == 50.0F);

    return 0;
}
