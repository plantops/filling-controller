// Angle from one index pulse per revolution.
//
// prox.start passes a fixed switching vane: one pulse per revolution, a true 0
// degree mark. Everything between two pulses is interpolated, and interpolation
// is only as good as the assumption that this revolution lasts as long as the
// last one. That assumption is checked, not trusted: when it fails the angle is
// marked Unusable and callers must not decide anything with it.
#pragma once
#include <cstdint>
#include "sp01/cp/params.hpp"

namespace sp01::cp {

enum class AngleQuality : std::uint8_t {
    Unknown,   // fewer than two index pulses seen
    Usable,    // period steady, angle may gate monitoring decisions
    Coasting,  // period moved more than the jitter limit: display only
    Lost,      // no index for index_lost_factor x nominal period
};

class AngleTracker {
public:
    explicit AngleTracker(const Params& p) : p_(p) {}

    // Feed the raw prox.start line every control tick.
    void update(bool index_raw, std::uint64_t now_us) {
        const std::uint64_t debounce_us = p_.index_debounce_ms.value * 1000ull;
        if (index_raw != raw_last_) { raw_last_ = index_raw; raw_change_us_ = now_us; }
        if (now_us - raw_change_us_ >= debounce_us && stable_ != raw_last_) {
            stable_ = raw_last_;
            if (stable_) on_rise(now_us); else on_fall(now_us);
        }
        const std::uint64_t lost_us =
            static_cast<std::uint64_t>(p_.index_lost_factor.value * p_.rev_period_nominal_ms.value) * 1000ull;
        if (rises_ == 0 || now_us - last_rise_us_ > lost_us) {
            if (rises_ > 0) quality_ = AngleQuality::Lost;
        }
        now_us_ = now_us;
    }

    AngleQuality quality() const { return quality_; }

    // 0..360. Only meaningful when quality() is Usable or Coasting.
    float degrees() const {
        if (rises_ < 2 || period_us_ == 0) return 0.f;
        const std::uint64_t dt = now_us_ - last_rise_us_;
        const float deg = 360.f * static_cast<float>(dt) / static_cast<float>(period_us_);
        return deg > 360.f ? 360.f : deg;
    }

    // True once this revolution has gone past the (provisional) discharge window
    // without the discharge prox firing. Diagnosis only: it never pushes a bag.
    bool past_discharge_window() const {
        if (quality() != AngleQuality::Usable) return false;
        return degrees() > p_.discharge_angle_deg.value + p_.discharge_angle_tol_deg.value;
    }

    std::uint64_t period_us() const { return period_us_; }
    std::uint32_t revolutions() const { return rises_; }

private:
    void on_rise(std::uint64_t now_us) {
        prev_rise_us_ = last_rise_us_;
        prev_period_us_ = period_us_;
        prev_quality_ = quality_;
        if (rises_ > 0) {
            const std::uint64_t dt = now_us - last_rise_us_;
            const std::uint64_t nom = p_.rev_period_nominal_ms.value * 1000ull;
            const std::uint64_t slack = nom * p_.rev_jitter_pct.value / 100ull;
            const bool steady = period_us_ != 0 &&
                                dt + slack >= period_us_ && dt <= period_us_ + slack;
            quality_ = (rises_ >= 1 && steady) ? AngleQuality::Usable : AngleQuality::Coasting;
            period_us_ = dt;
        }
        last_rise_us_ = now_us;
        ++rises_;
    }

    void on_fall(std::uint64_t now_us) {
        pulse_ms_ = static_cast<std::uint32_t>((now_us - last_rise_us_) / 1000ull);
        const bool plausible = pulse_ms_ >= p_.index_pulse_min_ms.value &&
                               pulse_ms_ <= p_.index_pulse_max_ms.value;
        if (plausible) return;
        // Too short to be the vane, or so long the sensor is stuck on. Either way
        // it was not a revolution, so take the rise back rather than let a spike
        // shift the zero mark. The angle is unusable until real pulses return.
        last_rise_us_ = prev_rise_us_;
        period_us_    = prev_period_us_;
        if (rises_ > 0) --rises_;
        quality_ = (rises_ == 0) ? AngleQuality::Unknown : AngleQuality::Lost;
    }

    const Params& p_;
    bool raw_last_{false}, stable_{false};
    std::uint64_t raw_change_us_{0}, last_rise_us_{0}, period_us_{0}, now_us_{0};
    std::uint64_t prev_rise_us_{0}, prev_period_us_{0};
    AngleQuality  prev_quality_{AngleQuality::Unknown};
    std::uint32_t rises_{0}, pulse_ms_{0};
    AngleQuality quality_{AngleQuality::Unknown};
};

}  // namespace sp01::cp
