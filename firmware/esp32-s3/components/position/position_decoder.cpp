#include "sp01/position_decoder.hpp"

namespace sp01 {
namespace {

constexpr float kFullTurnDeg = 360.0F;

std::uint8_t mark_bit(PositionMark mark) noexcept {
    return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(mark));
}

}  // namespace

const char* position_mark_name(PositionMark mark) noexcept {
    switch (mark) {
        case PositionMark::None: return "NONE";
        case PositionMark::CountUp: return "COUNT_UP";
        case PositionMark::CountDown: return "COUNT_DOWN";
        case PositionMark::Reset: return "RESET";
        case PositionMark::InitScanner: return "INIT_SCANNER";
        case PositionMark::Reject: return "REJECT";
    }
    return "NONE";
}

const char* position_fault_name(PositionFault fault) noexcept {
    switch (fault) {
        case PositionFault::None: return "NONE";
        case PositionFault::IndexMissing: return "INDEX_MISSING";
        case PositionFault::UnexpectedMark: return "UNEXPECTED_MARK";
        case PositionFault::DuplicateMark: return "DUPLICATE_MARK";
        case PositionFault::RevolutionTooFast: return "REVOLUTION_TOO_FAST";
        case PositionFault::RevolutionTooSlow: return "REVOLUTION_TOO_SLOW";
    }
    return "NONE";
}

void PositionDecoder::reset() noexcept {
    status_ = PositionStatus{};
    prev_index_ = false;
    prev_mark_ = false;
    last_index_edge_us_ = 0;
    last_mark_edge_us_ = 0;
    seen_mask_ = 0;
    have_index_ = false;
}

bool PositionDecoder::seen(PositionMark mark) const noexcept {
    return (seen_mask_ & mark_bit(mark)) != 0U;
}

void PositionDecoder::mark_seen(PositionMark mark) noexcept {
    seen_mask_ = static_cast<std::uint8_t>(seen_mask_ | mark_bit(mark));
}

PositionMark PositionDecoder::classify(std::uint64_t offset_us) const noexcept {
    // Scale the expected offsets by the measured revolution so the decoder
    // follows real shaft speed rather than assuming the nominal rate.
    const std::uint64_t rev = status_.revolution_us != 0
                                  ? status_.revolution_us
                                  : config_.nominal_revolution_us;
    const auto scale = [&](std::uint64_t nominal_offset) -> std::uint64_t {
        return static_cast<std::uint64_t>(
            (static_cast<double>(nominal_offset) * static_cast<double>(rev)) /
            static_cast<double>(config_.nominal_revolution_us));
    };
    const auto window =
        static_cast<std::uint64_t>(static_cast<double>(rev) *
                                   static_cast<double>(config_.window_fraction));

    struct Candidate {
        PositionMark mark;
        std::uint64_t nominal;
    };
    const Candidate candidates[] = {
        {PositionMark::CountDown, config_.count_down_us},
        {PositionMark::Reset, config_.reset_us},
        {PositionMark::InitScanner, config_.init_scanner_us},
        {PositionMark::Reject, config_.reject_us},
    };

    for (const auto& c : candidates) {
        const std::uint64_t expected = scale(c.nominal);
        const std::uint64_t lo = expected > window ? expected - window : 0;
        const std::uint64_t hi = expected + window;
        if (offset_us >= lo && offset_us <= hi) return c.mark;
    }
    return PositionMark::None;
}

PositionMark PositionDecoder::update(std::uint64_t now_us, bool index_level,
                                     bool mark_level) noexcept {
    PositionMark recognised = PositionMark::None;

    const bool index_edge = index_level && !prev_index_;
    const bool mark_edge = mark_level && !prev_mark_;
    prev_index_ = index_level;
    prev_mark_ = mark_level;

    if (index_edge && now_us - last_index_edge_us_ >= config_.debounce_us) {
        last_index_edge_us_ = now_us;
        if (have_index_) {
            const std::uint64_t rev = now_us - status_.last_index_us;
            if (rev < config_.min_revolution_us) {
                status_.fault = PositionFault::RevolutionTooFast;
                status_.synced = false;
            } else if (rev > config_.max_revolution_us) {
                status_.fault = PositionFault::RevolutionTooSlow;
                status_.synced = false;
            } else {
                status_.revolution_us = rev;
                status_.synced = true;
                status_.fault = PositionFault::None;
                ++status_.revolutions;
            }
        }
        have_index_ = true;
        status_.last_index_us = now_us;
        status_.last_mark = PositionMark::CountUp;
        status_.last_mark_us = now_us;
        seen_mask_ = 0;
        recognised = PositionMark::CountUp;
    }

    if (mark_edge && now_us - last_mark_edge_us_ >= config_.debounce_us) {
        last_mark_edge_us_ = now_us;
        if (!have_index_) {
            // A mark before any index tells us nothing about position.
            status_.fault = PositionFault::IndexMissing;
        } else {
            const PositionMark m = classify(now_us - status_.last_index_us);
            if (m == PositionMark::None) {
                status_.fault = PositionFault::UnexpectedMark;
                status_.synced = false;
            } else if (seen(m)) {
                status_.fault = PositionFault::DuplicateMark;
                status_.synced = false;
            } else {
                mark_seen(m);
                status_.last_mark = m;
                status_.last_mark_us = now_us;
                recognised = m;
            }
        }
    }

    // An index that never arrives is as serious as a wrong one.
    if (have_index_ && now_us - status_.last_index_us > config_.max_revolution_us) {
        status_.fault = PositionFault::IndexMissing;
        status_.synced = false;
    }

    if (status_.synced && status_.revolution_us != 0) {
        const std::uint64_t since = now_us - status_.last_index_us;
        float frac = static_cast<float>(since) /
                     static_cast<float>(status_.revolution_us);
        if (frac > 1.0F) frac = 1.0F;
        // Index sensor S4 sits at 315 deg.
        float deg = 315.0F + frac * kFullTurnDeg;
        while (deg >= kFullTurnDeg) deg -= kFullTurnDeg;
        status_.angle_deg = deg;
    }

    return recognised;
}

}  // namespace sp01
