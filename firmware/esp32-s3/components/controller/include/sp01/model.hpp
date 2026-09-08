#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sp01 {

enum class OperationMode : std::uint8_t {
    Auto,
    Manual,
};

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
    WaitDischarge,
    Push,
    Complete,
    Fault,
};

enum class Fault : std::uint8_t {
    None,
    PermissiveLost,
    BagMissing,
    BagLost,
    WeightStale,
    WeightFault,
    StateTimeout,
    IoFault,
    DischargeTimingInvalid,
    ModeChanged,
};

enum class Di : std::size_t {
    HopperFeederRunning = 0,
    DownstreamConveyorReady,
    MachineMotorRunning,
    ProcessInitiative,
    FillPosition,
    BagPresent,
    DischargeRefA,
    DischargeRefB,
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
    OperationMode mode{OperationMode::Auto};
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

struct ControllerConfig {
    float target_kg{50.0F};
    float coarse_to_fine_kg{40.0F};
    float cutoff_margin_kg{0.0F};

    std::uint64_t weight_stale_us{500000};
    std::uint64_t bag_acquire_timeout_us{2000000};
    std::uint64_t coarse_timeout_us{12000000};
    std::uint64_t fine_timeout_us{5000000};
    std::uint64_t settle_min_us{200000};
    std::uint64_t wait_discharge_timeout_us{6000000};
    std::uint64_t push_duration_us{500000};

    // A->B is normalized to 1000 counts. After B, countdown is
    // discharge_countdown_counts - discharge_lead_counts.
    std::uint32_t discharge_countdown_counts{1000};
    std::uint32_t discharge_lead_counts{0};
};

struct ControllerSnapshot {
    State state{State::WaitPermissive};
    Fault fault{Fault::None};
    OperationMode mode{OperationMode::Auto};
    OutputImage outputs{};
    std::uint64_t state_enter_us{0};
    std::uint64_t discharge_ref_interval_us{0};
    std::uint64_t discharge_due_us{0};
    std::uint32_t cycle_id{0};
};

constexpr OutputImage safe_output_image() noexcept { return {}; }

constexpr bool all_outputs_off(const OutputImage& image) noexcept {
    for (const bool value : image.channels) {
        if (value) return false;
    }
    return true;
}

constexpr bool input(const InputImage& image, Di channel) noexcept {
    return image.di[static_cast<std::size_t>(channel)];
}

constexpr bool output(const OutputImage& image, Do channel) noexcept {
    return image.channels[static_cast<std::size_t>(channel)];
}

constexpr void set_output(OutputImage& image, Do channel, bool value) noexcept {
    image.channels[static_cast<std::size_t>(channel)] = value;
}

constexpr bool auto_permissive(const InputImage& image) noexcept {
    return input(image, Di::HopperFeederRunning) &&
           input(image, Di::DownstreamConveyorReady) &&
           input(image, Di::MachineMotorRunning) &&
           input(image, Di::ProcessInitiative);
}

constexpr bool manual_fill_requested(const InputImage& image) noexcept {
    return input(image, Di::HopperFeederRunning) &&
           input(image, Di::ProcessInitiative);
}

const char* mode_name(OperationMode mode) noexcept;
const char* state_name(State state) noexcept;
const char* fault_name(Fault fault) noexcept;

}  // namespace sp01
