# SP01 G7 remote precheck — 2026-09-11

Status: **PRECHECK ONLY — G7 NOT PASS**

Branch: `diag/sp01-g2-g9`

Purpose: use remote time to reconcile canonical views with the executable without fabricating missing physical evidence.

Full adversarial review: [`REDTEAM_R1.md`](REDTEAM_R1.md).

## Evidence already available

```text
G0 PASS
G1 PASS
G2 1 h soak PASS
G2 DI1..DI8 PASS
G2 DO/reset-safe DEFERRED
G3 dry FSM PASS
```

G2T, G4, G5, G6 and G8/G9 physical evidence remain incomplete or blocked.

## Red-team R1 summary

```text
R1-001 CRITICAL  production reject-position adapter missing
R1-002 HIGH      production telemetry too thin for G8 evidence
R1-003 HIGH      broken-bag production config path incomplete
R1-004 HIGH      bench DO HTTP command lacks authentication
R1-005 HIGH      collector/runtime telemetry schema not frozen end-to-end
R1-006 MEDIUM    desired/commanded/field DO truth not distinguished
R1-007 MEDIUM    TLB sample timestamp is poll-start timestamp
R1-008 MEDIUM    service token uses plain HTTP transport
R1-009 MEDIUM    diagnostic artifact can be mistaken for production build
R1-010 OPEN      DO1/DO2 behavior after REJECT requires machine evidence
```

The most important blocker remains R1-001: the controller accepts `PositionSnapshot.reject_window`, but current production `app_main.cpp` calls `tick(now, inputs, weight)` without a production position adapter. The real 210-degree method must be measured/frozen in G8; do not invent a ninth DI.

`make_controller_config()` currently leaves broken-bag detector values disabled. This is correct while G4/G8 values are unknown, but all detector parameters including a finite reject-wait timeout must be introduced as one validated commissioning set before live authority.

## Gate status

```text
G7 = PENDING / RED-TEAM R1 COMPLETE, FINDINGS OPEN
G8 = BLOCKED
G9 = BLOCKED
```

Formal G7 PASS still requires the physical evidence owned by G2/G2T/G4/G5/G6 and closure or explicit evidence-based disposition of R1 findings.