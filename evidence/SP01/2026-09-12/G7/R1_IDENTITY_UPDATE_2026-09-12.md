# SP01 G7 R1 identity instrumentation update — 2026-09-12

Status: **software instrumentation added and CI PASS; no physical gate advanced**

Branch: `diag/sp01-g2-g9`
Implementation commit: `f030d1aba91094d550108590ec31a58859efc40d`
CI run: `34626849267`

## Change

Production `app_main.cpp` now logs commissioning identity at boot using ESP-IDF application metadata plus reset reason, followed by the effective controller/TLB configuration values used by the image.

The boot record includes:

```text
project name
application version
ESP-IDF version
first 8 bytes of ELF SHA-256
ESP reset reason
control period
target / coarse-to-fine / cutoff values
weight stale limit
broken-bag loss / persistence / reject timeout tuple
discharge countdown / lead
DI and DO invert masks
TLB enabled flag / baud / slave / poll period
```

No service token or Wi-Fi password is logged.

## Purpose

This closes the boot-log portion of R1-002/R1-009 identity instrumentation. Physical evidence can now capture the exact running firmware identity, reset cause and effective commissioning configuration rather than relying only on an artifact filename or operator memory.

This does **not** close the remaining R1-002 position/timing telemetry work and does not prove any physical gate.

## CI evidence

GitHub Actions run `34626849267` for implementation commit `f030d1aba91094d550108590ec31a58859efc40d` completed successfully:

```text
linux-amd64  PASS
  validate gate tools    PASS
  host configure/build   PASS
  controller tests       PASS
  host smoke             PASS

esp32-s3     PASS
  ESP-IDF v5.5.5 build   PASS
  flash bundle assembly  PASS
  artifact upload        PASS
```

CI/simulation evidence is software evidence only.

## Gate impact

```text
G2  ACTIVE — unchanged
G2T BLOCKED-HW
G3  PASS
G4  BLOCKED-HW
G5  BLOCKED-HW
G6  PENDING
G7  PENDING — R1-001 remains critical; prerequisites incomplete
G8  BLOCKED-HW — outputs remain isolated
G9  BLOCKED-HW
```

Current next physical G2 exit evidence remains approved dummy-load DO1..DO8 one-hot OFF -> pulse ON -> auto OFF, followed by reset/restart physical safe-output verification with machine actuator wiring disconnected.
