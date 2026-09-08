#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sp01 {

enum class State : std::uint8_t {
    WaitPermissive,
    WaitFillPosition,
    BagAcquire,
    BagVerify,
    TareReady,
    CoarseFill,
    FineFill,
    Cutoff,
    Settle,
    WaitPushPosition,
    Push,
    Complete,
    Fault,
};

enum class Di : std::size_t {
    HopperFeederRunning = 0,
    DownstreamConveyorReady,
    MachineMotorRunning,
    ProcessInitiative,
    FillPosition,
    BagPresent,
    PushPosition,
    Spare,
};

enum class Do : std::size_t {
    ScannerDown = 0,
    BagDetectAir,
    BagPush,
    DosingValveA,
    DosingValveB,
    DosingValveC,
    FillingMotor,
    SpoutAeration,
};

enum class WeightQuality : std::uint8_t {
    Unknown,
    Good,
    Stale,
    Fault,
};

struct InputImage {
    std::array<bool, 8> di{};
};

struct OutputImage {
    std::array<bool, 8> channels{};
};

struct WeightSnapshot {
    float net_kg{0.0F};
    std::uint64_t sample_time_us{0};
    std::uint32_t sequence{0};
    WeightQuality quality{WeightQuality::Unknown};
    bool stable{false};
};

constexpr OutputImage safe_output_image() noexcept {
    return {};
}

constexpr bool all_outputs_off(const OutputImage& image) noexcept {
    for (const bool value : image.channels) {
        if (value) {
            return false;
        }
    }
    return true;
}

}  // namespace sp01
