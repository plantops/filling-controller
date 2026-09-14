#include "sp01/plausibility.hpp"
#include <cstdio>
using namespace sp01;
namespace {
int fails=0;
void check(bool ok,const char* w){std::printf("%s  %s\n",ok?"PASS":"FAIL",w);if(!ok)++fails;}
InputImage mk(bool motor,bool idx,bool mk_){
  InputImage in{};
  in.di[static_cast<std::size_t>(Di::MachineMotorRunning)]=motor;
  in.di[static_cast<std::size_t>(Di::PositionIndex)]=idx;
  in.di[static_cast<std::size_t>(Di::PositionMark)]=mk_;
  return in;
}
// run a healthy machine: index once per revolution, a mark shortly after
void healthy(PlausibilityMonitor& m,std::uint64_t& t,int revs){
  for(int r=0;r<revs;r++){
    m.update(t,mk(true,true,false)); t+=20000;
    m.update(t,mk(true,false,false)); t+=980000;
    m.update(t,mk(true,false,true)); t+=20000;
    m.update(t,mk(true,false,false)); t+=13380000;
  }
}
}
int main(){
  { // healthy machine stays ready
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t,mk(true,false,false)); t+=10000;
    healthy(m,t,4);
    check(m.ready(), "healthy machine reports ready");
  }
  { // both position lines at once
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t,mk(true,false,false)); t+=10000;
    m.update(t,mk(true,true,true));
    check(!m.ready(), "not ready when both position inputs are on");
    check(m.status().violated(Implausible::PositionIndexAndMarkTogether),
          "index+mark together is flagged");
    check(m.status().first==Implausible::PositionIndexAndMarkTogether,
          "first violation is recorded");
  }
  { // pulse while motor is reported stopped
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t,mk(false,false,false)); t+=10000;
    m.update(t,mk(false,true,false));
    check(m.status().violated(Implausible::IndexWhileMotorStopped),
          "index pulse with the motor stopped is flagged");
  }
  { // motor running, index never arrives
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t,mk(true,false,false));
    for(int i=0;i<80;i++){ t+=1000000; m.update(t,mk(true,false,false)); }
    check(m.status().violated(Implausible::IndexMissingWhileRunning),
          "missing index while running is flagged");
  }
  { // a position line welded high
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t,mk(true,false,false)); t+=10000;
    for(int i=0;i<60;i++){ t+=1000000; m.update(t,mk(true,true,false)); }
    check(m.status().violated(Implausible::PositionInputStuckHigh),
          "a stuck-high position input is flagged");
  }
  { // machine stopped: none of the rotation rules may fire
    PlausibilityMonitor m; std::uint64_t t=1000000;
    for(int i=0;i<200;i++){ t+=1000000; m.update(t,mk(false,false,false)); }
    check(m.ready(), "a stopped machine is not faulted for not turning");
  }
  { // clear does not hide a live fault
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t,mk(true,false,false)); t+=10000;
    m.update(t,mk(true,true,true)); t+=10000;
    check(!m.ready(), "faulted");
    m.clear();
    check(m.ready(), "ready right after clear");
    m.update(t,mk(true,true,true));
    check(!m.ready(), "latches again because the fault is still present");
  }
  { // every code has both languages
    bool all=true;
    for(std::uint16_t i=1;i<static_cast<std::uint16_t>(Implausible::Count);++i){
      const auto c=static_cast<Implausible>(i);
      if(implausible_text_en(c)[0]=='\0'||implausible_text_vi(c)[0]=='\0') all=false;
    }
    check(all, "every rule has EN and VI text");
  }
  std::printf("\n%s\n", fails==0?"ALL PASS":"FAILURES");
  return fails==0?0:1;
}
