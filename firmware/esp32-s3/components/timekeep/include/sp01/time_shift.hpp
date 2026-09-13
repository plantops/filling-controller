#pragma once

// Wall-clock time and shift accounting for SP01.
//
// The board has no battery-backed RTC. Time comes from the browser: whenever an
// HMI page loads it posts its local clock, and the device holds the offset
// against its own monotonic timer.
//
// The consequence must be handled honestly. After a power cut, and until
// somebody opens the HMI, the device does not know the time. In that window it
// must not reset shift or day counters, because it cannot tell which shift a bag
// belongs to. Bags produced while unsynced are counted into a separate
// unattributed bucket and the HMI says so.
//
// Pure logic, no ESP-IDF dependency, covered by the host test suite.

#include <cstdint>

namespace sp01 {

enum class TimeSource : std::uint8_t {
    None = 0,   // never synced since boot
    Browser,    // an HMI page supplied the clock
};

// Shifts change at 00:00, 08:00 and 16:00 local time.
constexpr std::uint8_t kShiftsPerDay = 3;
constexpr std::uint8_t kShiftHours = 8;

struct CivilTime {
    std::uint16_t year{0};
    std::uint8_t month{0};
    std::uint8_t day{0};
    std::uint8_t hour{0};
    std::uint8_t minute{0};
    std::uint8_t second{0};
};

struct ShiftCounters {
    std::uint32_t bags{0};
    std::uint32_t rejects{0};
    float kg{0.0F};
};

class TimeKeeper {
   public:
    // unix_ms is the browser's clock; tz_offset_min is its UTC offset, so the
    // device can compute local civil time without carrying a timezone database.
    void sync_from_browser(std::uint64_t now_monotonic_us, std::uint64_t unix_ms,
                           std::int16_t tz_offset_min) noexcept;

    bool synced() const noexcept { return source_ != TimeSource::None; }
    TimeSource source() const noexcept { return source_; }

    // Seconds since the Unix epoch, local time. Zero when unsynced.
    std::uint64_t local_epoch_s(std::uint64_t now_monotonic_us) const noexcept;

    CivilTime civil(std::uint64_t now_monotonic_us) const noexcept;

    // 1..3, or 0 when unsynced.
    std::uint8_t shift_no(std::uint64_t now_monotonic_us) const noexcept;

    // Days since epoch, local. Zero when unsynced.
    std::uint32_t local_day(std::uint64_t now_monotonic_us) const noexcept;

   private:
    TimeSource source_{TimeSource::None};
    std::uint64_t local_epoch_us_at_sync_{0};
    std::uint64_t monotonic_at_sync_{0};
    std::int16_t tz_offset_min_{0};
};

// Owns the counters and rolls them over when the shift or day changes.
class ShiftTracker {
   public:
    // Call every tick. Rollover only happens when the clock is known.
    void update(const TimeKeeper& clock, std::uint64_t now_monotonic_us) noexcept;

    void record_bag(float net_kg, bool rejected) noexcept;

    const ShiftCounters& shift() const noexcept { return shift_; }
    const ShiftCounters& day() const noexcept { return day_; }

    // Bags counted while the clock was unknown. Never silently folded into a
    // shift: the HMI shows them separately so nobody reads a wrong shift total.
    const ShiftCounters& unattributed() const noexcept { return unattributed_; }

    std::uint8_t current_shift() const noexcept { return current_shift_; }
    std::uint32_t current_day() const noexcept { return current_day_; }
    bool attributing() const noexcept { return attributing_; }

   private:
    ShiftCounters shift_{};
    ShiftCounters day_{};
    ShiftCounters unattributed_{};
    std::uint8_t current_shift_{0};
    std::uint32_t current_day_{0};
    bool attributing_{false};
};

}  // namespace sp01
