# SP01 G7 — field telemetry integrity: DEV simulation guard

Date: 2026-10-26
Branch: `diag/sp01-g2-g9`
Implementation: `5c53a264a2801bbbf3d4cff1a70bdf6c3d0f77c6`
Status: software change committed; firmware CI/build **NOT VERIFIED**; G7 remains PENDING.

## Finding

The embedded HMI exposed `POST /api/dev/cmd` without a build-profile restriction. A remote `op=mode&value=FULL_SW` request could select simulated snapshots for the shared `GET /api/state` endpoint and suspend the hardware trace collector in `trace_task`. This could contaminate field-shadow evidence without changing controller/board output authority.

## Focused correction

In `firmware/esp32-s3/components/web_hmi/web_hmi.cpp`, reject all DEV simulation commands with HTTP 403 unless `CONFIG_SP01_BENCH_DO_TEST_ENABLE` is enabled at compile time. The existing bench profile is explicitly for disconnected dummy-load work; the field profile in `sdkconfig.defaults` leaves this option disabled.

No controller FSM, TLB acquisition, physical I/O, process DO suppression, calibration service route, or G8-to-G9 authority was modified.

## Verification and limits

- Read back the committed file and confirmed the compile-time guard precedes command parsing.
- Confirmed the denied branch returns HTTP 403 and the original command handler is retained under the isolated-bench compile-time branch.
- The current default field profile sets dummy weight and suppresses normal process DO; the bench DO option is disabled.
- **Not a runtime test:** no ESP-IDF build, flashed-device test, or SHA-specific CI PASS was available at this evidence record.
- In a deliberately enabled bench profile, DEV simulation still shares the HMI snapshot/trace paths. Its outputs must not be used as physical commissioning evidence.

## Gate decision

No gate advance. G2T, G4, G5, G6, G7, G8 and G9 remain subject to the documented physical/review exit evidence. This source correction is not physical proof. Machine actuators remain disconnected until formal G8 acceptance and local authorization of the G8-to-G9 transition.
