#include "sp01/trace.hpp"
#include <cstdio>
using namespace sp01;
namespace {
int fails=0;
void check(bool ok,const char* w){std::printf("%s  %s\n",ok?"PASS":"FAIL",w);if(!ok)++fails;}
}
int main(){
  TraceBuffer buf; TraceRecorder rec;
  InputImage in{}; ControllerSnapshot snap{}; OutputImage cmd{};

  rec.observe(1000, in, snap, cmd, false, buf);
  check(buf.size() > 0, "initial picture is recorded");
  const std::uint32_t after_prime = buf.last_seq();

  // A pulse far shorter than any poll interval must survive.
  snap.outputs.channels[static_cast<std::size_t>(Do::BagPush)] = true;
  rec.observe(2000, in, snap, cmd, false, buf);
  snap.outputs.channels[static_cast<std::size_t>(Do::BagPush)] = false;
  rec.observe(2020, in, snap, cmd, false, buf);   // 20 us wide

  TraceEvent ev[64]; std::uint32_t lost=0;
  std::size_t n = buf.since(after_prime, ev, 64, &lost);
  check(n == 2, "both edges of a 20 us pulse recorded");
  check(ev[0].new_value==1 && ev[1].new_value==0, "rising then falling");
  check(ev[1].t_us - ev[0].t_us == 20, "pulse width preserved exactly");
  check(lost == 0, "nothing reported lost");

  // Unchanged ticks add nothing: the trace is edges, not samples.
  const std::uint32_t before_idle = buf.last_seq();
  for (int i=0;i<100;i++) rec.observe(3000+i*10, in, snap, cmd, false, buf);
  check(buf.last_seq() == before_idle, "idle ticks add no events");

  // State and disposition transitions are captured.
  snap.state = State::Push;
  snap.disposition = BagDisposition::Reject;
  rec.observe(4000, in, snap, cmd, false, buf);
  n = buf.since(before_idle, ev, 64, &lost);
  bool st=false, dp=false;
  for (std::size_t i=0;i<n;i++){
    if (ev[i].kind==TraceKind::StateChange) st=true;
    if (ev[i].kind==TraceKind::Disposition) dp=true;
  }
  check(st, "state change recorded");
  check(dp, "disposition change recorded");

  // Overflow must be reported, not hidden.
  TraceBuffer small; 
  for (std::uint32_t i=0;i<kTraceCapacity+50;i++)
    small.add(i, TraceKind::Di, 0, 0, 1);
  n = small.since(0, ev, 64, &lost);
  check(small.size()==kTraceCapacity, "buffer caps at capacity");
  check(lost >= 50, "overwritten events are reported as lost");

  // A client that is up to date gets nothing.
  n = buf.since(buf.last_seq(), ev, 64, &lost);
  check(n==0, "no events after the caller's last sequence");

  std::printf("\n%s\n", fails==0?"ALL PASS":"FAILURES");
  return fails==0?0:1;
}
