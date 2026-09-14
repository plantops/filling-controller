#include "sp01/plausibility.hpp"

namespace sp01 {
namespace {
std::uint16_t bit_of(Implausible c) noexcept {
    return static_cast<std::uint16_t>(1U << static_cast<std::uint16_t>(c));
}
}  // namespace

const char* implausible_name(Implausible code) noexcept {
    switch (code) {
        case Implausible::None: return "NONE";
        case Implausible::PositionIndexAndMarkTogether: return "INDEX_AND_MARK_TOGETHER";
        case Implausible::IndexWhileMotorStopped: return "INDEX_WHILE_MOTOR_STOPPED";
        case Implausible::IndexMissingWhileRunning: return "INDEX_MISSING_WHILE_RUNNING";
        case Implausible::PositionInputStuckHigh: return "POSITION_INPUT_STUCK_HIGH";
        case Implausible::PositionInputDead: return "POSITION_INPUT_DEAD";
        case Implausible::FillPositionStuckHigh: return "FILL_POSITION_STUCK_HIGH";
        case Implausible::WeightRisingWithFeederOff: return "WEIGHT_RISING_FEEDER_OFF";
        case Implausible::Count: break;
    }
    return "UNKNOWN";
}

const char* implausible_text_en(Implausible code) noexcept {
    switch (code) {
        case Implausible::None: return "";
        case Implausible::PositionIndexAndMarkTogether:
            return "Both position inputs active at once: check wiring, they cannot overlap";
        case Implausible::IndexWhileMotorStopped:
            return "Shaft is turning but the main motor reports stopped";
        case Implausible::IndexMissingWhileRunning:
            return "Motor running but no index pulse for over one revolution";
        case Implausible::PositionInputStuckHigh:
            return "A position input has stayed on for more than one revolution";
        case Implausible::PositionInputDead:
            return "A position input has not changed over several revolutions";
        case Implausible::FillPositionStuckHigh:
            return "Fill position input stuck on: it is a pulse, not a level";
        case Implausible::WeightRisingWithFeederOff:
            return "Weight is rising while the hopper feeder is stopped";
        case Implausible::Count: break;
    }
    return "";
}

const char* implausible_text_vi(Implausible code) noexcept {
    switch (code) {
        case Implausible::None: return "";
        case Implausible::PositionIndexAndMarkTogether:
            return "Hai đầu vào vị trí cùng tác động: kiểm tra dây, chúng không thể trùng nhau";
        case Implausible::IndexWhileMotorStopped:
            return "Trục đang quay nhưng động cơ chính báo đã dừng";
        case Implausible::IndexMissingWhileRunning:
            return "Động cơ chạy nhưng quá một vòng không có xung index";
        case Implausible::PositionInputStuckHigh:
            return "Một đầu vào vị trí dính ở mức ON quá một vòng quay";
        case Implausible::PositionInputDead:
            return "Một đầu vào vị trí không đổi trạng thái qua nhiều vòng quay";
        case Implausible::FillPositionStuckHigh:
            return "Đầu vào vị trí nạp dính mức ON: đây là xung, không phải mức";
        case Implausible::WeightRisingWithFeederOff:
            return "Cân đang tăng trong khi cấp liệu phễu đã dừng";
        case Implausible::Count: break;
    }
    return "";
}

void PlausibilityMonitor::reset() noexcept {
    status_ = PlausibilityStatus{};
    primed_ = false;
    index_high_since_us_ = 0;
    mark_high_since_us_ = 0;
    last_index_edge_us_ = 0;
    last_mark_edge_us_ = 0;
    motor_started_us_ = 0;
}

void PlausibilityMonitor::clear() noexcept {
    status_.flags = 0;
    status_.first = Implausible::None;
    status_.first_seen_us = 0;
    status_.ready = true;
}

void PlausibilityMonitor::raise(Implausible code, std::uint64_t now_us) noexcept {
    if (status_.violated(code)) return;
    status_.flags = static_cast<std::uint16_t>(status_.flags | bit_of(code));
    status_.ready = false;
    if (status_.first == Implausible::None) {
        status_.first = code;
        status_.first_seen_us = now_us;
    }
}

void PlausibilityMonitor::update(std::uint64_t now_us, const InputImage& inputs,
                                 const WeightSnapshot& weight) noexcept {
    const bool index = input(inputs, Di::PositionIndex);
    const bool mark = input(inputs, Di::PositionMark);
    const bool motor = input(inputs, Di::MachineMotorRunning);
    const bool fill_pos = input(inputs, Di::FillPosition);
    const bool feeder = input(inputs, Di::HopperFeederRunning);

    if (!primed_) {
        primed_ = true;
        prev_index_ = index;
        prev_mark_ = mark;
        prev_motor_ = motor;
        index_high_since_us_ = index ? now_us : 0;
        mark_high_since_us_ = mark ? now_us : 0;
        last_index_edge_us_ = now_us;
        last_mark_edge_us_ = now_us;
        motor_started_us_ = motor ? now_us : 0;
        prev_fill_pos_ = fill_pos;
        prev_feeder_ = feeder;
        fill_pos_high_since_us_ = fill_pos ? now_us : 0;
        feeder_off_ref_kg_ = weight.net_kg;
        feeder_off_since_us_ = now_us;
        return;
    }

    // Rule 1: the sensors are 25 deg apart at the closest; they cannot overlap.
    if (index && mark) raise(Implausible::PositionIndexAndMarkTogether, now_us);

    const bool index_edge = index != prev_index_;
    const bool mark_edge = mark != prev_mark_;

    if (index_edge && now_us - last_index_edge_us_ >= cfg_.debounce_us) {
        last_index_edge_us_ = now_us;
        index_high_since_us_ = index ? now_us : 0;
        // Rule 2: a pulse arriving while the motor is reported stopped is a
        // contradiction. Either the motor signal is wrong or the shaft is being
        // turned by something else.
        if (index && !motor) raise(Implausible::IndexWhileMotorStopped, now_us);
    }
    if (mark_edge && now_us - last_mark_edge_us_ >= cfg_.debounce_us) {
        last_mark_edge_us_ = now_us;
        mark_high_since_us_ = mark ? now_us : 0;
    }

    if (motor && !prev_motor_) motor_started_us_ = now_us;

    if (motor) {
        const std::uint64_t running_for = now_us - motor_started_us_;
        // Give the shaft a revolution to produce its first pulse after start.
        if (running_for > cfg_.max_revolution_us) {
            // Rule 3.
            if (now_us - last_index_edge_us_ > cfg_.max_revolution_us) {
                raise(Implausible::IndexMissingWhileRunning, now_us);
            }
            // Rule 5: a line that never moves while the machine turns is dead.
            const std::uint64_t dead_after =
                cfg_.max_revolution_us * cfg_.dead_input_revolutions;
            if (now_us - last_mark_edge_us_ > dead_after) {
                raise(Implausible::PositionInputDead, now_us);
            }
        }
    }

    // Rule 4: these are pulses, not levels.
    if (index && index_high_since_us_ != 0 &&
        now_us - index_high_since_us_ > cfg_.max_revolution_us) {
        raise(Implausible::PositionInputStuckHigh, now_us);
    }
    if (mark && mark_high_since_us_ != 0 &&
        now_us - mark_high_since_us_ > cfg_.max_revolution_us) {
        raise(Implausible::PositionInputStuckHigh, now_us);
    }

    // Rule 6: DI5 is a pulse.
    if (fill_pos != prev_fill_pos_) fill_pos_high_since_us_ = fill_pos ? now_us : 0;
    if (fill_pos && fill_pos_high_since_us_ != 0 &&
        now_us - fill_pos_high_since_us_ > cfg_.max_revolution_us) {
        raise(Implausible::FillPositionStuckHigh, now_us);
    }

    // Rule 7: nothing should be entering the bag with the feeder stopped.
    if (feeder || weight.quality != WeightQuality::Good) {
        feeder_off_ref_kg_ = weight.net_kg;
        feeder_off_since_us_ = now_us;
    } else {
        if (weight.net_kg < feeder_off_ref_kg_) {
            // A fall resets the reference: only a sustained rise matters.
            feeder_off_ref_kg_ = weight.net_kg;
            feeder_off_since_us_ = now_us;
        } else if (weight.net_kg - feeder_off_ref_kg_ >= cfg_.feeder_off_rise_kg &&
                   now_us - feeder_off_since_us_ >= cfg_.feeder_off_window_us) {
            raise(Implausible::WeightRisingWithFeederOff, now_us);
        }
    }

    prev_index_ = index;
    prev_mark_ = mark;
    prev_motor_ = motor;
    prev_fill_pos_ = fill_pos;
    prev_feeder_ = feeder;
}

}  // namespace sp01
