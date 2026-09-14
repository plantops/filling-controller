#include "sp01/test_source.hpp"
#include <cstdio>
using namespace sp01;
namespace {
int fails=0;
void check(bool ok,const char* w){std::printf("%s  %s\n",ok?"PASS":"FAIL",w);if(!ok)++fails;}
}
int main(){
  TestSource ts;
  check(ts.mode()==RunMode::RealHw, "starts in REAL_HW");
  check(ts.output_authority_possible(), "REAL_HW can hold output authority");

  // Gating.
  check(ts.request_mode(RunMode::FullSw, State::CoarseFill, true)==ModeChange::NotIdle,
        "mode change refused mid-cycle");
  // A faulted controller is not mid-cycle. Refusing here trapped the operator:
  // a fault raised inside FULL_SW made leaving FULL_SW impossible.
  check(ts.request_mode(RunMode::FullSw, State::Fault, true)==ModeChange::Ok,
        "mode change allowed from FAULT");
  ts.request_mode(RunMode::RealHw, State::Fault, true);
  check(ts.request_mode(RunMode::Simu, State::Complete, true)==ModeChange::Ok,
        "mode change allowed from COMPLETE");
  ts.request_mode(RunMode::RealHw, State::WaitPermissive, true);
  check(ts.request_mode(RunMode::FullSw, State::FineFill, true)==ModeChange::NotIdle,
        "mode change still refused mid-fill");
  check(ts.request_mode(RunMode::FullSw, State::WaitPermissive, false)==ModeChange::BadPin,
        "mode change refused with a wrong PIN");
  check(ts.request_mode(RunMode::FullSw, State::WaitPermissive, true)==ModeChange::Ok,
        "mode change accepted when idle with the right PIN");
  check(ts.mode()==RunMode::FullSw, "mode is FULL_SW");

  // The safety rule that has no opt-out.
  check(!ts.output_authority_possible(), "FULL_SW can never hold output authority");
  ts.request_mode(RunMode::Simu, State::WaitPermissive, true);
  check(!ts.output_authority_possible(), "SIMU can never hold output authority");

  // RealHw passes hardware through untouched.
  ts.request_mode(RunMode::RealHw, State::WaitPermissive, true);
  SourceImages hw{};
  hw.weight.net_kg = 12.5F;
  hw.position.angle_deg = 77.0F;
  hw.inputs.di[static_cast<std::size_t>(Di::BagPresent)] = true;
  SourceImages got = ts.apply(1000, 10000, hw, State::WaitPermissive);
  check(got.weight.net_kg == 12.5F && got.position.angle_deg == 77.0F,
        "REAL_HW passes hardware through unchanged");

  // FullSw holds whatever the operator set, and moves only when they move it.
  ts.request_mode(RunMode::FullSw, State::WaitPermissive, true);
  ts.set_weight_kg(25.0F);
  ts.set_angle_deg(200.0F);
  ts.set_di(static_cast<std::size_t>(Di::BagPresent), true);
  SourceImages a = ts.apply(2000, 10000, hw, State::CoarseFill);
  SourceImages b = ts.apply(3000, 10000, hw, State::CoarseFill);
  check(a.weight.net_kg == 25.0F && b.weight.net_kg == 25.0F,
        "FULL_SW weight does not drift by itself");
  check(b.position.angle_deg == 200.0F, "FULL_SW angle stays where it was put");
  check(b.inputs.di[static_cast<std::size_t>(Di::BagPresent)],
        "FULL_SW input held");
  check(b.weight.sequence != a.weight.sequence,
        "weight sequence still advances so freshness checks work");

  // Simu runs on its own.
  ts.request_mode(RunMode::Simu, State::WaitPermissive, true);
  SourceImages s1 = ts.apply(4000, 100000, hw, State::CoarseFill);
  SourceImages s2 = ts.apply(4100, 100000, hw, State::CoarseFill);
  check(s2.weight.net_kg > s1.weight.net_kg, "SIMU weight rises while filling");
  check(s2.position.angle_deg > s1.position.angle_deg, "SIMU shaft turns");
  const float dw = s2.weight.net_kg - s1.weight.net_kg;
  check(dw > 0.7F && dw < 0.9F, "coarse rate is about 8 kg/s");

  SourceImages i1 = ts.apply(5000, 100000, hw, State::Settle);
  SourceImages i2 = ts.apply(5100, 100000, hw, State::Settle);
  check(i2.weight.net_kg == i1.weight.net_kg, "SIMU weight holds when not filling");
  check(i2.weight.stable, "SIMU reports stable when not filling");

  // Pausing freezes the plant.
  ts.set_running(false);
  SourceImages p1 = ts.apply(6000, 100000, hw, State::CoarseFill);
  SourceImages p2 = ts.apply(6100, 100000, hw, State::CoarseFill);
  check(p2.position.angle_deg == p1.position.angle_deg, "SIMU pause stops the shaft");
  check(p2.weight.net_kg == p1.weight.net_kg, "SIMU pause stops filling");

  std::printf("\n%s\n", fails==0?"ALL PASS":"FAILURES");
  return fails==0?0:1;
}
