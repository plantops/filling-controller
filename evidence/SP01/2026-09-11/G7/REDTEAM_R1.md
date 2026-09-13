# SP01 G7 Red-Team R1 — remote static pass

Date: 2026-09-11
Branch: `diag/sp01-g2-g9`
Status: **G7 NOT PASS — open critical/high findings remain**

Scope: reconcile V1..V13 against current controller, production adapter, local HMI, TLB adapter, edge collector and available bench evidence. This is a static/remote review only; it does not substitute for G2/G2T/G4/G5/G6 physical evidence.

## Severity rule

- CRITICAL: blocks G8/G9 authority or can create an unsafe control path.
- HIGH: blocks trustworthy commissioning evidence or leaves an important control/service boundary incomplete.
- MEDIUM: must be resolved or explicitly accepted before G9, but does not by itself create live authority.

## Findings

### R1-001 — CRITICAL — production reject-position adapter missing

Canonical reject flow requires `PositionSnapshot.reject_window` near 210 degrees. The shared controller supports this input, but current production `app_main.cpp` calls `tick(now, inputs, weight)` without a `PositionSnapshot`; default `reject_window=false` means production cannot execute `REJECT_WAIT -> PUSH` from real rotor position evidence.

Required closure: during G8 determine the as-built 210-degree timing/reference from the existing machine, implement a production position adapter, pass it into `Controller::tick`, and capture shadow evidence. Do not invent a ninth DI.

### R1-002 — HIGH — production telemetry is too thin for V1/V8/V10/G8 evidence

Current local `/api/state` exposes mode/state/fault/cycle/weight/stable/quality/DI/DO and basic TLB counters, but omits key canonical evidence fields already present in `ControllerSnapshot` or required by V10: disposition, broken-bag detect timestamp/peak/detected weight, discharge interval/due time, weight sequence/sample age, firmware/config identity, uptime/reset reason, reject-window state, and explicit desired-vs-commanded physical DO distinction.

Consequence: the engineering console and collector cannot reconstruct the required GOOD/REJECT timeline reliably from the production API.

Required closure: extend the read-only runtime snapshot contract before G8 shadow. Missing fields must render unavailable rather than be fabricated.

### R1-003 — HIGH — no commissionable production path yet for broken-bag configuration

`ControllerConfig` contains `broken_bag_loss_trip_kg`, `broken_bag_persist_us` and `reject_wait_timeout_us`, all disabled by default. Current `Kconfig.projbuild` and `make_controller_config()` do not provide production values for them. This is acceptable while G4/G8 values are unknown, but it means there is currently no production configuration path to enable the detector after measurement without another code change.

Risk if enabled incompletely: detector values could be added later while `reject_wait_timeout_us` remains zero, leaving a rejected cycle in `REJECT_WAIT` indefinitely if the 210-degree event is absent.

Required closure: after G4/G8 measurements, add all three values as one validated commissioning set. Reject enabling the detector unless loss trip, persistence and reject-wait timeout are all valid together.

### R1-004 — HIGH — bench DO HTTP command lacks authentication

`/api/bench/do` is registered as POST when the bench flag is enabled, but unlike calibration writes it does not call `token_ok()` or `service_request_allowed()`. The compile-time flag defaults OFF and the current dedicated G2 loopback diagnostic does not expose this HMI, so this is not evidence of a present machine command. It is nevertheless an avoidable service-path weakness: an accidentally enabled bench build on a reachable LAN would accept unauthenticated DO pulses.

Required closure: require a non-empty service token at minimum, preferably an explicit bench profile plus local/service interlock. Keep the endpoint absent in production profiles.

### R1-005 — HIGH — collector event keys are not a frozen canonical schema

Collector change tracking currently watches `cycle` and `broken_detected_us`, while the canonical V10 naming uses `cycle_id` and `broken_bag_detected_us`. Current local HMI itself emits `cycle`, and the desired future API fields are not frozen end-to-end.

Consequence: event history can silently miss cycle/reject transitions when the local API is upgraded unless aliases/schema versioning are handled deliberately.

Required closure: freeze one telemetry schema/version and make local API, collector and engineering console consume the same names. Add tests for cycle and broken-bag event generation.

### R1-006 — MEDIUM — physical DO truth is not distinguishable from desired/commanded DO

The canonical views distinguish desired DO from physical DO when available. Current local HMI packs `ControllerSnapshot.outputs` as `do`. `BoardIo` tracks the last byte successfully written to the TCA9554 internally, but this is not exported. There is also no field-side feedback, so the word "physical" would be misleading.

Required closure: expose separate `desired_do_bits` and `commanded_do_bits` (last successful expander write). Treat actual field actuation as verified only by G2/G8 physical evidence or dedicated feedback, not by the software latch value.

### R1-007 — MEDIUM — TLB sample timestamp is assigned from poll-start time

`Tlb485::poll_once(now_us)` records `WeightSnapshot.sample_time_us = now_us`, where `now_us` is supplied before the Modbus transaction. The transaction duration is measured separately. Therefore computed sample age can be understated by the poll/response duration.

Required closure: G4 must quantify latency/jitter. Prefer timestamping the successful sample at receive/validation completion, while retaining poll duration diagnostics.

### R1-008 — MEDIUM — service token travels over plain HTTP

Calibration writes use a service token header but the embedded HMI is HTTP. On a trusted isolated plant VLAN this may be an accepted deployment constraint; on a routed/untrusted network the token is observable to a network attacker.

Required closure: document network trust boundary for G9. Do not expose service-write endpoints through the Cloudflare/static supervisory path. If service traffic crosses an untrusted segment, add a protected transport/tunnel or equivalent network control.

### R1-009 — MEDIUM — commissioning diagnostic artifact can be mistaken for production firmware

The commissioning branch intentionally switches `main/CMakeLists.txt` between diagnostic entry points such as G2 DI/DO builds. This is useful for bench work but creates a human-factor risk if a diagnostic artifact is later treated as the production G8/G9 image.

Required closure: production artifact must expose an explicit build profile/firmware SHA/config revision in telemetry and boot log, and G8/G9 evidence must record that identity.

### R1-010 — OPEN MACHINE QUESTION — scanner/bag-detect behavior after REJECT

Current controller keeps DO1 scanner and DO2 bag-detect air active in `REJECT_WAIT`. Only DO4..DO8 immediate shutdown is confirmed. This is not classified as a code defect yet because the real machine sequence has not been measured.

Required closure: G8 shadow must compare legacy behavior after broken-bag decision and either freeze DO1/DO2 behavior or change the state output map from measured evidence.

## What passed this static pass

- Controller core keeps REJECT separate from FAULT.
- Broken-bag detector is disabled by default; simulation thresholds are not production defaults.
- REJECT removes DO4..DO8 in the same controller decision and latches disposition in host tests.
- GOOD and REJECT ejection routes are separate in the controller tests.
- Edge collector has GET/SSE read-only routes; no actuator write route is present there.
- Cloud/static engineering console states supervisory-only intent and does not own controller timing.
- G2 physical DI1..DI8 evidence is complete; DO/reset-safe remains deferred, not falsely passed.

## R1 verdict

```text
CRITICAL open: 1   R1-001
HIGH open:     4   R1-002..R1-005
MEDIUM/open:   5   R1-006..R1-010

G7 = NOT PASS
G8 = BLOCKED by gate prerequisites and R1-001
G9 = BLOCKED
```

Remote software work may close R1-002, R1-004, R1-005, R1-006 and build-profile portions of R1-009 without claiming any physical gate. R1-001, R1-003 numeric values, R1-007 measurement acceptance and R1-010 require later G4/G8 evidence.