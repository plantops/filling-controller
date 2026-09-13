#pragma once

// Decodes the SP01 rotary position sensors from two digital inputs.
//
// Five sensors sit on one shaft, one pulse per revolution each. DI7 carries the
// index sensor alone (S4 COUNT UP, 315 deg). DI8 carries the other four wired in
// parallel, so the board sees their logical OR. They are separated by their time
// offset from the index pulse, which at the nominal 14.4 s revolution is at
// least one second between neighbours.
//
// The decoder never guesses. If the index is missing, if an extra pulse arrives,
// or if a mark falls outside every expected window, it reports invalid and the
// caller drives outputs to the safe state.
//
// Pure logic, no ESP-IDF dependency, so it is covered by the host test suite.

#include <cstddef>
#include <cstdint>

namespace sp01 {

enum class PositionMark : std::uint8_t {
    None = 0,
    CountUp,     // S4, index, 315 deg
    CountDown,   // S5, 340 deg
    Reset,       // S1, 10 deg
    InitScanner, // S2, 55 deg
    Reject,      // S3, 210 deg
};

const char* position_mark_name(PositionMark mark) noexcept;

enum class PositionFault : std::uint8_t {
    None = 0,
    IndexMissing,     // no index within the allowed revolution window
    UnexpectedMark,   // a mark outside every window
    DuplicateMark,    // the same mark seen twice in one revolution
    RevolutionTooFast,
    RevolutionTooSlow,
};

const char* position_fault_name(PositionFault fault) noexcept;

struct PositionConfig {
    // Nominal revolution at rated output: 14.4 s per bag per spout.
    std::uint64_t nominal_revolution_us{14400000};

    // A revolution outside these bounds is rejected rather than decoded.
    std::uint64_t min_revolution_us{7200000};   // 2x rated speed
    std::uint64_t max_revolution_us{28800000};  // half rated speed

    // Expected offsets after the index pulse, at nominal speed.
    std::uint64_t count_down_us{1000000};
    std::uint64_t reset_us{2200000};
    std::uint64_t init_scanner_us{4000000};
    std::uint64_t reject_us{10200000};

    // Acceptance half-width around each expected offset, as a fraction of the
    // measured revolution. 0.03 of 14.4 s is +/- 432 ms, comfortably inside the
    // ~1 s minimum separation.
    float window_fraction{0.03F};

    // Ignore edges closer together than this (contact bounce).
    std::uint64_t debounce_us{5000};
};

struct PositionStatus {
    bool synced{false};             // at least two clean index pulses seen
    PositionFault fault{PositionFault::None};
    PositionMark last_mark{PositionMark::None};
    std::uint64_t last_mark_us{0};
    std::uint64_t last_index_us{0};
    std::uint64_t revolution_us{0}; // measured, 0 until synced
    std::uint32_t revolutions{0};
    float angle_deg{0.0F};          // interpolated from time since index
};

class PositionDecoder {
   public:
    PositionDecoder() = default;
    explicit PositionDecoder(const PositionConfig& config) noexcept
        : config_(config) {}

    void reset() noexcept;

    // Feed the raw levels each control tick. Returns the mark recognised on
    // this tick, or None. Faults are reported through status().
    PositionMark update(std::uint64_t now_us, bool index_level,
                        bool mark_level) noexcept;

    const PositionStatus& status() const noexcept { return status_; }
    const PositionConfig& config() const noexcept { return config_; }

    // True when the decoder is synced and no fault is latched.
    bool usable() const noexcept {
        return status_.synced && status_.fault == PositionFault::None;
    }

   private:
    PositionMark classify(std::uint64_t offset_us) const noexcept;
    bool seen(PositionMark mark) const noexcept;
    void mark_seen(PositionMark mark) noexcept;

    PositionConfig config_{};
    PositionStatus status_{};
    bool prev_index_{false};
    bool prev_mark_{false};
    std::uint64_t last_index_edge_us_{0};
    std::uint64_t last_mark_edge_us_{0};
    std::uint8_t seen_mask_{0};
    bool have_index_{false};
};

}  // namespace sp01
