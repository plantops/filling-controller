// SP01 filling controller — parameters.
// Every value carries where it came from. A provisional number that looks like a
// measured one is the most dangerous thing in this system, so Source is not
// optional and not stored anywhere but next to the value itself.
#pragma once
#include <cstdint>

namespace sp01::cp {

enum class Source : std::uint8_t {
    Measured,      // measured on the machine
    Manufacturer,  // Claudius Peters manual, page recorded in note
    Provisional,   // chosen by software, nobody has checked it
};

template <typename T>
struct P {
    T value;
    Source source;
    const char* note;
};

// --- recipe -----------------------------------------------------------------
struct Recipe {
    const char* name;        // "50.1", "40.3"
    float target_kg;         // net target
    float coarse_to_fine_kg; // switch main feed -> dribble feed
    float tol_plus_kg;
    float tol_minus_kg;
};

// --- machine ----------------------------------------------------------------
struct Params {
    // weighing
    P<float>         after_flow_kg          {0.50f, Source::Manufacturer, "CP workshop value, manual p.316"};
    P<std::uint32_t> settle_ms              {400,   Source::Manufacturer, "standstill time 0.4 s, manual p.320"};
    P<std::uint32_t> weight_lost_ms         {500,   Source::Provisional,  "no new TLB reading"};
    P<float>         empty_bag_kg           {0.20f, Source::Measured,     "stated by plant"};

    // index / angle
    P<std::uint32_t> index_debounce_ms      {20,    Source::Provisional,  "too small: double count; too large: missed vane"};
    P<std::uint32_t> index_pulse_min_ms     {50,    Source::Provisional,  "below this it is noise, not a revolution"};
    P<std::uint32_t> index_pulse_max_ms     {600,   Source::Provisional,  "above this the vane or sensor is stuck"};
    P<std::uint32_t> rev_period_nominal_ms  {15000, Source::Measured,     "stated by plant"};
    P<std::uint8_t>  rev_jitter_pct         {10,    Source::Provisional,  "beyond this the angle is display-only"};
    P<float>         index_lost_factor      {1.5f,  Source::Provisional,  "no index for this many nominal periods"};
    P<float>         discharge_angle_deg    {350.f, Source::Provisional,  "monitoring only, never gates the push"};
    P<float>         discharge_angle_tol_deg{30.f,  Source::Provisional,  "monitoring only"};

    // filling
    P<std::uint32_t> bag_verify_ms          {1500,  Source::Provisional,  "clamp to pressure build-up"};
    P<std::uint32_t> blow_out_ms            {300,   Source::Provisional,  "timed impulse, manual p.157"};
    P<std::uint32_t> fill_timeout_ms        {12000, Source::Provisional,  "must stay under one revolution"};
    P<float>         bag_break_min_kg_s     {2.0f,  Source::Provisional,  "fill rate floor"};
    P<std::uint32_t> bag_break_delay_ms     {1500,  Source::Manufacturer, "delay bag rupture recognition, manual p.317"};

    // turbine (SP01 owns the contactor)
    P<std::uint32_t> turbine_start_delay_ms{200,   Source::Provisional,  "gate opens first, then the impeller"};

    // discharge
    P<std::uint32_t> push_ms                {500,   Source::Provisional,  "bag discharge cylinder pulse"};
    P<std::uint32_t> bag_clear_timeout_ms   {2000,  Source::Provisional,  "bag_present must drop after the push"};
    P<std::uint32_t> wait_discharge_ms      {20000, Source::Provisional,  "> one revolution: a bag may wait for its own push point"};
};

// True when any decision taken from this value would rest on a guess.
constexpr bool is_provisional(Source s) { return s == Source::Provisional; }

}  // namespace sp01::cp
