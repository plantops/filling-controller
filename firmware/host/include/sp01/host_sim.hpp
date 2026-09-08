#pragma once

#include "sp01/model.hpp"

#include <cstddef>
#include <cstdint>

namespace sp01::host {

class ManualClock {
public:
    [[nodiscard]] std::uint64_t now_us() const noexcept { return now_us_; }
    void advance_us(std::uint64_t delta_us) noexcept { now_us_ += delta_us; }

private:
    std::uint64_t now_us_{0};
};

class VirtualIo {
public:
    void set_mode(OperationMode mode) noexcept { inputs_.mode = mode; }

    void set_input(Di channel, bool value) noexcept {
        inputs_.di[static_cast<std::size_t>(channel)] = value;
    }

    [[nodiscard]] bool input_value(Di channel) const noexcept {
        return inputs_.di[static_cast<std::size_t>(channel)];
    }

    [[nodiscard]] const InputImage& read_inputs() const noexcept { return inputs_; }

    void commit_outputs(const OutputImage& image) noexcept { outputs_ = image; }

    [[nodiscard]] bool output_value(Do channel) const noexcept {
        return outputs_.channels[static_cast<std::size_t>(channel)];
    }

    [[nodiscard]] const OutputImage& outputs() const noexcept { return outputs_; }

private:
    InputImage inputs_{};
    OutputImage outputs_{safe_output_image()};
};

class VirtualWeigher {
public:
    void publish(float net_kg,
                 bool stable,
                 WeightQuality quality,
                 std::uint64_t sample_time_us) noexcept {
        snapshot_.net_kg = net_kg;
        snapshot_.stable = stable;
        snapshot_.quality = quality;
        snapshot_.sample_time_us = sample_time_us;
        ++snapshot_.sequence;
    }

    [[nodiscard]] const WeightSnapshot& latest() const noexcept { return snapshot_; }

private:
    WeightSnapshot snapshot_{};
};

}  // namespace sp01::host
