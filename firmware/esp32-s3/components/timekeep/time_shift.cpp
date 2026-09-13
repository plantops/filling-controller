#include "sp01/time_shift.hpp"

namespace sp01 {
namespace {

constexpr std::uint64_t kUsPerSec = 1000000ULL;
constexpr std::uint64_t kSecPerDay = 86400ULL;

// Civil-from-days, Howard Hinnant's algorithm. Valid for the Gregorian calendar.
void civil_from_days(std::int64_t z, std::uint16_t& y, std::uint8_t& m,
                     std::uint8_t& d) noexcept {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const std::uint64_t doe = static_cast<std::uint64_t>(z - era * 146097);
    const std::uint64_t yoe =
        (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t yy = static_cast<std::int64_t>(yoe) + era * 400;
    const std::uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::uint64_t mp = (5 * doy + 2) / 153;
    const std::uint64_t dd = doy - (153 * mp + 2) / 5 + 1;
    const std::uint64_t mm = mp < 10 ? mp + 3 : mp - 9;
    y = static_cast<std::uint16_t>(yy + (mm <= 2 ? 1 : 0));
    m = static_cast<std::uint8_t>(mm);
    d = static_cast<std::uint8_t>(dd);
}

}  // namespace

void TimeKeeper::sync_from_browser(std::uint64_t now_monotonic_us,
                                   std::uint64_t unix_ms,
                                   std::int16_t tz_offset_min) noexcept {
    tz_offset_min_ = tz_offset_min;
    const std::int64_t offset_us =
        static_cast<std::int64_t>(tz_offset_min) * 60LL *
        static_cast<std::int64_t>(kUsPerSec);
    const std::int64_t local_us =
        static_cast<std::int64_t>(unix_ms) * 1000LL + offset_us;
    local_epoch_us_at_sync_ =
        local_us > 0 ? static_cast<std::uint64_t>(local_us) : 0;
    monotonic_at_sync_ = now_monotonic_us;
    source_ = TimeSource::Browser;
}

std::uint64_t TimeKeeper::local_epoch_s(
    std::uint64_t now_monotonic_us) const noexcept {
    if (!synced()) return 0;
    const std::uint64_t elapsed = now_monotonic_us >= monotonic_at_sync_
                                      ? now_monotonic_us - monotonic_at_sync_
                                      : 0;
    return (local_epoch_us_at_sync_ + elapsed) / kUsPerSec;
}

CivilTime TimeKeeper::civil(std::uint64_t now_monotonic_us) const noexcept {
    CivilTime c{};
    if (!synced()) return c;
    const std::uint64_t s = local_epoch_s(now_monotonic_us);
    const std::uint64_t days = s / kSecPerDay;
    const std::uint64_t rem = s % kSecPerDay;
    civil_from_days(static_cast<std::int64_t>(days), c.year, c.month, c.day);
    c.hour = static_cast<std::uint8_t>(rem / 3600);
    c.minute = static_cast<std::uint8_t>((rem % 3600) / 60);
    c.second = static_cast<std::uint8_t>(rem % 60);
    return c;
}

std::uint8_t TimeKeeper::shift_no(std::uint64_t now_monotonic_us) const noexcept {
    if (!synced()) return 0;
    const std::uint8_t hour = civil(now_monotonic_us).hour;
    return static_cast<std::uint8_t>(hour / kShiftHours + 1);
}

std::uint32_t TimeKeeper::local_day(
    std::uint64_t now_monotonic_us) const noexcept {
    if (!synced()) return 0;
    return static_cast<std::uint32_t>(local_epoch_s(now_monotonic_us) / kSecPerDay);
}

void ShiftTracker::update(const TimeKeeper& clock,
                          std::uint64_t now_monotonic_us) noexcept {
    if (!clock.synced()) {
        // No rollover while the time is unknown: a wrong reset loses a shift's
        // production, which is worse than a late one.
        attributing_ = false;
        return;
    }

    const std::uint8_t sh = clock.shift_no(now_monotonic_us);
    const std::uint32_t day = clock.local_day(now_monotonic_us);

    if (!attributing_) {
        // First sync after boot: adopt the current period without clearing the
        // counters, so a mid-shift reconnect does not erase what was counted.
        current_shift_ = sh;
        current_day_ = day;
        attributing_ = true;
        return;
    }

    if (day != current_day_) {
        day_ = ShiftCounters{};
        current_day_ = day;
    }
    if (sh != current_shift_) {
        shift_ = ShiftCounters{};
        current_shift_ = sh;
    }
}

void ShiftTracker::record_bag(float net_kg, bool rejected) noexcept {
    ShiftCounters* targets[2] = {&shift_, &day_};
    if (!attributing_) {
        ++unattributed_.bags;
        unattributed_.kg += net_kg;
        if (rejected) ++unattributed_.rejects;
        return;
    }
    for (auto* c : targets) {
        ++c->bags;
        c->kg += net_kg;
        if (rejected) ++c->rejects;
    }
}

}  // namespace sp01
