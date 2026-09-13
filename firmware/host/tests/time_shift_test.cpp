#include "sp01/time_shift.hpp"
#include <cstdio>
using namespace sp01;
namespace {
int fails=0;
void check(bool ok,const char* w){std::printf("%s  %s\n",ok?"PASS":"FAIL",w);if(!ok)++fails;}
// 2026-09-13 14:32:07 UTC+7  ->  unix ms
constexpr std::uint64_t kUnixMs = 1789284727000ULL;  // 2026-09-13 14:32:07 at UTC+7
}
int main(){
  TimeKeeper tk;
  check(!tk.synced(), "unsynced at boot");
  check(tk.shift_no(0)==0, "no shift number while unsynced");

  tk.sync_from_browser(5000000, kUnixMs, 420);  // UTC+7
  check(tk.synced(), "synced after browser post");
  auto c = tk.civil(5000000);
  std::printf("      civil = %04u-%02u-%02u %02u:%02u:%02u\n",
    c.year,c.month,c.day,c.hour,c.minute,c.second);
  check(c.year==2026 && c.month==9 && c.day==13, "civil date decoded");
  check(c.hour==14, "local hour is 14 at UTC+7");
  check(tk.shift_no(5000000)==2, "14:00 falls in shift 2");

  // Shift boundaries.
  TimeKeeper b;
  b.sync_from_browser(0, kUnixMs - 14ULL*3600*1000 - 32*60*1000 - 7000, 420);
  check(b.civil(0).hour==0, "midnight decoded");
  check(b.shift_no(0)==1, "00:00 is shift 1");
  check(b.shift_no(8ULL*3600*1000000ULL)==2, "08:00 starts shift 2");
  check(b.shift_no(16ULL*3600*1000000ULL)==3, "16:00 starts shift 3");

  // Counters must not roll over while the clock is unknown.
  ShiftTracker st; TimeKeeper un;
  st.update(un, 1000);
  st.record_bag(50.2F,false);
  st.record_bag(50.1F,true);
  check(st.shift().bags==0, "no shift count while unsynced");
  check(st.unattributed().bags==2, "bags go to the unattributed bucket");
  check(st.unattributed().rejects==1, "rejects tracked while unsynced");

  // After sync, counting is attributed but nothing already counted is erased.
  TimeKeeper ok; ok.sync_from_browser(0, kUnixMs, 420);
  st.update(ok, 0);
  check(st.attributing(), "attributing after sync");
  st.record_bag(50.2F,false);
  check(st.shift().bags==1 && st.day().bags==1, "bag counted into shift and day");
  check(st.unattributed().bags==2, "unattributed bucket preserved");

  // Crossing into the next shift clears the shift, not the day.
  ShiftTracker s2; TimeKeeper t2;
  t2.sync_from_browser(0, kUnixMs, 420);          // 14:32, shift 2
  s2.update(t2,0);
  s2.record_bag(50.0F,false);
  s2.record_bag(50.0F,true);
  check(s2.shift().bags==2 && s2.day().bags==2, "two bags counted");
  TimeKeeper t3; t3.sync_from_browser(0, kUnixMs + 2ULL*3600*1000, 420); // 16:32
  s2.update(t3,0);
  check(s2.shift().bags==0, "shift counters cleared at shift change");
  check(s2.day().bags==2, "day counters survive shift change");
  check(s2.day().rejects==1, "day rejects survive");

  std::printf("\n%s\n", fails==0?"ALL PASS":"FAILURES");
  return fails==0?0:1;
}
