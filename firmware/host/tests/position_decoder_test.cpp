#include "sp01/position_decoder.hpp"
#include <cassert>
#include <cstdio>
using namespace sp01;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}
// Feed the decoder from t0 to t1 in 10 ms ticks with both lines low.
void idle(PositionDecoder& d, std::uint64_t& t, std::uint64_t dur) {
    const std::uint64_t end = t + dur;
    while (t < end) { d.update(t, false, false); t += 10000; }
}
void pulse(PositionDecoder& d, std::uint64_t& t, bool index) {
    d.update(t, index, !index); t += 20000;
    d.update(t, false, false);  t += 10000;
}
}  // namespace

int main() {
    PositionConfig cfg{};
    PositionDecoder d{cfg};
    std::uint64_t t = 1000000;

    // Two clean revolutions at nominal speed.
    for (int rev = 0; rev < 3; ++rev) {
        pulse(d, t, true);                 // index / COUNT_UP at offset 0
        idle(d, t, 1000000 - 30000); pulse(d, t, false);   // COUNT_DOWN
        idle(d, t, 1200000 - 30000); pulse(d, t, false);   // RESET
        idle(d, t, 1800000 - 30000); pulse(d, t, false);   // INIT_SCANNER
        idle(d, t, 6200000 - 30000); pulse(d, t, false);   // REJECT
        idle(d, t, 4200000 - 30000);
    }
    check(d.usable(), "synced after clean revolutions");
    check(d.status().fault == PositionFault::None, "no fault on clean input");
    check(d.status().revolutions >= 2, "revolution counter advances");
    const auto rev_us = d.status().revolution_us;
    check(rev_us > 14000000 && rev_us < 14800000, "revolution ~14.4 s");

    // A mark in a gap belongs to no sensor.
    PositionDecoder d2{cfg};
    std::uint64_t t2 = 1000000;
    pulse(d2, t2, true);
    idle(d2, t2, 6000000); pulse(d2, t2, false);  // 6 s: between scanner and reject
    check(d2.status().fault == PositionFault::UnexpectedMark,
          "mark in a gap raises UNEXPECTED_MARK");
    check(!d2.usable(), "unusable after unexpected mark");

    // Missing index for longer than max revolution.
    PositionDecoder d3{cfg};
    std::uint64_t t3 = 1000000;
    pulse(d3, t3, true);
    idle(d3, t3, 30000000);
    check(d3.status().fault == PositionFault::IndexMissing,
          "missing index raises INDEX_MISSING");

    // Same mark twice inside one revolution.
    PositionDecoder d4{cfg};
    std::uint64_t t4 = 1000000;
    pulse(d4, t4, true);
    idle(d4, t4, 1000000 - 30000); pulse(d4, t4, false);
    idle(d4, t4, 100000); pulse(d4, t4, false);
    check(d4.status().fault == PositionFault::DuplicateMark,
          "repeated mark raises DUPLICATE_MARK");

    std::printf("\n%s\n", failures == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
