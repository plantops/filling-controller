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
WeightSnapshot W(float kg=0.0F){ WeightSnapshot w{}; w.net_kg=kg; w.quality=WeightQuality::Good; return w; }
void healthy(PlausibilityMonitor& m,std::uint64_t& t,int revs){
  for(int r=0;r<revs;r++){
    m.update(t, mk(true,true,false), W()); t+=20000;
    m.update(t, mk(true,false,false), W()); t+=980000;
    m.update(t, mk(true,false,true), W()); t+=20000;
    m.update(t, mk(true,false,false), W()); t+=13380000;
  }
}
}
int main(){
  { // healthy machine stays ready
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t, mk(true,false,false), W()); t+=10000;
    healthy(m,t,4);
    check(m.ready(), "healthy machine reports ready");
  }
  { // both position lines at once
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t, mk(true,false,false), W()); t+=10000;
    m.update(t, mk(true,true,true), W());
    check(!m.ready(), "not ready when both position inputs are on");
    check(m.status().violated(Implausible::PositionIndexAndMarkTogether),
          "index+mark together is flagged");
    check(m.status().first==Implausible::PositionIndexAndMarkTogether,
          "first violation is recorded");
  }
  { // pulse while motor is reported stopped
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t, mk(false,false,false), W()); t+=10000;
    m.update(t, mk(false,true,false), W());
    check(m.status().violated(Implausible::IndexWhileMotorStopped),
          "index pulse with the motor stopped is flagged");
  }
  { // motor running, index never arrives
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t, mk(true,false,false), W());
    for(int i=0;i<80;i++){ t+=1000000; m.update(t, mk(true,false,false), W()); }
    check(m.status().violated(Implausible::IndexMissingWhileRunning),
          "missing index while running is flagged");
  }
  { // a position line welded high
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t, mk(true,false,false), W()); t+=10000;
    for(int i=0;i<60;i++){ t+=1000000; m.update(t, mk(true,true,false), W()); }
    check(m.status().violated(Implausible::PositionInputStuckHigh),
          "a stuck-high position input is flagged");
  }
  { // machine stopped: none of the rotation rules may fire
    PlausibilityMonitor m; std::uint64_t t=1000000;
    for(int i=0;i<200;i++){ t+=1000000; m.update(t, mk(false,false,false), W()); }
    check(m.ready(), "a stopped machine is not faulted for not turning");
  }
  { // clear does not hide a live fault
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t, mk(true,false,false), W()); t+=10000;
    m.update(t, mk(true,true,true), W()); t+=10000;
    check(!m.ready(), "faulted");
    m.clear();
    check(m.ready(), "ready right after clear");
    m.update(t, mk(true,true,true), W());
    check(!m.ready(), "latches again because the fault is still present");
  }
  { // DI5 is a pulse, not a level
    PlausibilityMonitor m; std::uint64_t t=1000000;
    m.update(t, mk(true,false,false), W()); t+=10000;
    for(int i=0;i<60;i++){
      t+=1000000;
      InputImage in=mk(true,false,false);
      in.di[static_cast<std::size_t>(Di::FillPosition)]=true;
      m.update(t,in,W());
    }
    check(m.status().violated(Implausible::FillPositionStuckHigh),
          "fill position stuck high is flagged");
  }
  { // cement arriving with the feeder off
    PlausibilityMonitor m; std::uint64_t t=1000000;
    InputImage in=mk(true,false,false);   // feeder OFF
    m.update(t,in,W(10.0F)); t+=10000;
    for(int i=0;i<30;i++){ t+=100000; m.update(t,in,W(10.0F+0.1F*i)); }
    check(m.status().violated(Implausible::WeightRisingWithFeederOff),
          "weight rising with the feeder off is flagged");
  }
  { // the same rise with the feeder running is normal
    PlausibilityMonitor m; std::uint64_t t=1000000;
    InputImage in=mk(true,false,false);
    in.di[static_cast<std::size_t>(Di::HopperFeederRunning)]=true;
    m.update(t,in,W(10.0F)); t+=10000;
    for(int i=0;i<30;i++){ t+=100000; m.update(t,in,W(10.0F+0.3F*i)); }
    check(m.ready(), "filling with the feeder running is not a fault");
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
