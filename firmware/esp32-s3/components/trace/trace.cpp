#include "sp01/trace.hpp"

namespace sp01 {

const char* trace_kind_name(TraceKind kind) noexcept {
    switch (kind) {
        case TraceKind::Di: return "DI";
        case TraceKind::Do: return "DO";
        case TraceKind::DoCommanded: return "DOC";
        case TraceKind::StateChange: return "STATE";
        case TraceKind::FaultChange: return "FAULT";
        case TraceKind::Disposition: return "BAG";
        case TraceKind::PositionValid: return "POS";
        case TraceKind::TargetChange: return "TARGET";
    }
    return "?";
}

void TraceBuffer::clear() noexcept {
    head_ = 0;
    count_ = 0;
    last_seq_ = 0;
}

void TraceBuffer::add(std::uint64_t t_us, TraceKind kind, std::uint8_t channel,
                      std::int32_t old_value, std::int32_t new_value) noexcept {
    TraceEvent& slot = events_[head_];
    slot.seq = ++last_seq_;
    slot.t_us = t_us;
    slot.kind = kind;
    slot.channel = channel;
    slot.old_value = old_value;
    slot.new_value = new_value;

    head_ = (head_ + 1) % kTraceCapacity;
    if (count_ < kTraceCapacity) ++count_;
}

std::size_t TraceBuffer::since(std::uint32_t since_seq, TraceEvent* out,
                               std::size_t cap, std::uint32_t* lost) const noexcept {
    if (lost != nullptr) *lost = 0;
    if (out == nullptr || cap == 0 || count_ == 0) return 0;

    // Oldest retained event.
    const std::size_t start = (head_ + kTraceCapacity - count_) % kTraceCapacity;
    const std::uint32_t oldest_seq = events_[start].seq;

    if (lost != nullptr && since_seq + 1 < oldest_seq) {
        *lost = oldest_seq - (since_seq + 1);
    }

    std::size_t written = 0;
    for (std::size_t i = 0; i < count_ && written < cap; ++i) {
        const TraceEvent& e = events_[(start + i) % kTraceCapacity];
        if (e.seq > since_seq) out[written++] = e;
    }
    return written;
}

void TraceRecorder::reset() noexcept {
    primed_ = false;
    prev_target_g_ = -1;
}

void TraceRecorder::observe(std::uint64_t t_us, const InputImage& inputs,
                            const ControllerSnapshot& snapshot,
                            const OutputImage& commanded, bool position_valid,
                            TraceBuffer& out) noexcept {
    if (!primed_) {
        // Record the starting picture so a trace read from the beginning is
        // interpretable without assuming everything started at zero.
        for (std::size_t i = 0; i < inputs.di.size(); ++i) {
            out.add(t_us, TraceKind::Di, static_cast<std::uint8_t>(i), -1,
                    inputs.di[i] ? 1 : 0);
        }
        for (std::size_t i = 0; i < snapshot.outputs.channels.size(); ++i) {
            out.add(t_us, TraceKind::Do, static_cast<std::uint8_t>(i), -1,
                    snapshot.outputs.channels[i] ? 1 : 0);
            out.add(t_us, TraceKind::DoCommanded, static_cast<std::uint8_t>(i), -1,
                    commanded.channels[i] ? 1 : 0);
        }
        out.add(t_us, TraceKind::StateChange, 0, -1,
                static_cast<std::int32_t>(snapshot.state));
        out.add(t_us, TraceKind::PositionValid, 0, -1, position_valid ? 1 : 0);

        prev_inputs_ = inputs;
        prev_desired_ = snapshot.outputs;
        prev_commanded_ = commanded;
        prev_state_ = snapshot.state;
        prev_fault_ = snapshot.fault;
        prev_disposition_ = snapshot.disposition;
        prev_position_valid_ = position_valid;
        primed_ = true;
        return;
    }

    for (std::size_t i = 0; i < inputs.di.size(); ++i) {
        if (inputs.di[i] != prev_inputs_.di[i]) {
            out.add(t_us, TraceKind::Di, static_cast<std::uint8_t>(i),
                    prev_inputs_.di[i] ? 1 : 0, inputs.di[i] ? 1 : 0);
        }
    }
    for (std::size_t i = 0; i < snapshot.outputs.channels.size(); ++i) {
        if (snapshot.outputs.channels[i] != prev_desired_.channels[i]) {
            out.add(t_us, TraceKind::Do, static_cast<std::uint8_t>(i),
                    prev_desired_.channels[i] ? 1 : 0,
                    snapshot.outputs.channels[i] ? 1 : 0);
        }
        if (commanded.channels[i] != prev_commanded_.channels[i]) {
            out.add(t_us, TraceKind::DoCommanded, static_cast<std::uint8_t>(i),
                    prev_commanded_.channels[i] ? 1 : 0,
                    commanded.channels[i] ? 1 : 0);
        }
    }
    if (snapshot.state != prev_state_) {
        out.add(t_us, TraceKind::StateChange, 0,
                static_cast<std::int32_t>(prev_state_),
                static_cast<std::int32_t>(snapshot.state));
    }
    if (snapshot.fault != prev_fault_) {
        out.add(t_us, TraceKind::FaultChange, 0,
                static_cast<std::int32_t>(prev_fault_),
                static_cast<std::int32_t>(snapshot.fault));
    }
    if (snapshot.disposition != prev_disposition_) {
        out.add(t_us, TraceKind::Disposition, 0,
                static_cast<std::int32_t>(prev_disposition_),
                static_cast<std::int32_t>(snapshot.disposition));
    }
    if (position_valid != prev_position_valid_) {
        out.add(t_us, TraceKind::PositionValid, 0, prev_position_valid_ ? 1 : 0,
                position_valid ? 1 : 0);
    }

    prev_inputs_ = inputs;
    prev_desired_ = snapshot.outputs;
    prev_commanded_ = commanded;
    prev_state_ = snapshot.state;
    prev_fault_ = snapshot.fault;
    prev_disposition_ = snapshot.disposition;
    prev_position_valid_ = position_valid;
}

void TraceRecorder::observe_target(std::uint64_t t_us, float target_kg,
                                   TraceBuffer& out) noexcept {
    const std::int32_t grams = static_cast<std::int32_t>(target_kg * 1000.0F + 0.5F);
    if (grams != prev_target_g_) {
        out.add(t_us, TraceKind::TargetChange, 0, prev_target_g_, grams);
        prev_target_g_ = grams;
    }
}

}  // namespace sp01
