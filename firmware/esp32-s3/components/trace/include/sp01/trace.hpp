#pragma once

// Event trace, recorded at control-tick rate inside the firmware.
//
// Why this is not done in the browser: the DEV page used to derive its timeline
// by diffing two polls 300 ms apart, while a push pulse lasts 500 ms and other
// outputs are shorter still. Anything narrower than the poll interval simply
// never appeared, and the whole trace was lost on page reload. A timeline built
// that way can suggest that DO3 fired only in its window; it cannot show it.
//
// Recording here, on every tick, makes the acceptance criteria checkable:
// "DO3 only during its valid push window" and "no double push" become
// statements about recorded transitions rather than about what a poll happened
// to catch.
//
// Pure logic, no ESP-IDF dependency, so the host test suite covers it.

#include <array>
#include <cstddef>
#include <cstdint>

#include "sp01/model.hpp"

namespace sp01 {

enum class TraceKind : std::uint8_t {
    Di = 0,     // channel = Di index, values 0/1
    Do,         // channel = Do index, values 0/1 (desired)
    DoCommanded,// channel = Do index, values 0/1 (what reached the adapter)
    StateChange,// values = State
    FaultChange,// values = Fault
    Disposition,// values = BagDisposition
    PositionValid,
    TargetChange,  // values in grams, so the record stays integral
};

const char* trace_kind_name(TraceKind kind) noexcept;

struct TraceEvent {
    std::uint32_t seq{0};
    std::uint64_t t_us{0};
    TraceKind kind{TraceKind::Di};
    std::uint8_t channel{0};
    std::int32_t old_value{0};
    std::int32_t new_value{0};
};

// 1024 events at 8 DI + 8 DO + state changes is several complete bag cycles.
constexpr std::size_t kTraceCapacity = 1024;

class TraceBuffer {
   public:
    void clear() noexcept;
    void add(std::uint64_t t_us, TraceKind kind, std::uint8_t channel,
             std::int32_t old_value, std::int32_t new_value) noexcept;

    // Events with seq > since, oldest first. Returns how many were written.
    // `lost` reports events that were overwritten before the caller read them,
    // so a client is told when its view has a hole instead of silently
    // receiving a partial picture.
    std::size_t since(std::uint32_t since_seq, TraceEvent* out, std::size_t cap,
                      std::uint32_t* lost) const noexcept;

    std::uint32_t last_seq() const noexcept { return last_seq_; }
    std::size_t size() const noexcept { return count_; }

   private:
    std::array<TraceEvent, kTraceCapacity> events_{};
    std::size_t head_{0};
    std::size_t count_{0};
    std::uint32_t last_seq_{0};
};

// Diffs successive control-tick images and appends only what changed.
class TraceRecorder {
   public:
    void reset() noexcept;

    void observe(std::uint64_t t_us, const InputImage& inputs,
                 const ControllerSnapshot& snapshot,
                 const OutputImage& commanded, bool position_valid,
                 TraceBuffer& out) noexcept;

    void observe_target(std::uint64_t t_us, float target_kg,
                        TraceBuffer& out) noexcept;

   private:
    bool primed_{false};
    InputImage prev_inputs_{};
    OutputImage prev_desired_{};
    OutputImage prev_commanded_{};
    State prev_state_{State::WaitPermissive};
    Fault prev_fault_{Fault::None};
    BagDisposition prev_disposition_{BagDisposition::Undecided};
    bool prev_position_valid_{false};
    std::int32_t prev_target_g_{-1};
};

}  // namespace sp01
