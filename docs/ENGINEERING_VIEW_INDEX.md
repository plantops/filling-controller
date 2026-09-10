# SP01 Engineering View Index

Canonical engineering views for design review, commissioning, HMI and long-term maintenance.

The original Yellow-Team 11-view proposal is retained only as design input. Current executable/physical truth has precedence.

| View | Canonical purpose | Source |
|---|---|---|
| V1 | Runtime timeline | `SP01_ENGINEERING_VIEWS.md` + `BROKEN_BAG_REJECT.md` |
| V2 | State logic matrix | `SP01_ENGINEERING_VIEWS.md` |
| V3 | Interlock flow | `SP01_ENGINEERING_VIEWS.md` |
| V4 | Interlock equations; K-map explicitly non-canonical | `SP01_ENGINEERING_VIEWS.md` |
| V5 | Logic dependency graph | `SP01_ENGINEERING_VIEWS.md` |
| V6 | Executable C++ engine / ground truth | `SP01_ENGINEERING_VIEWS.md` + controller source |
| V7 | Physical I/O / terminals | `SP01_ENGINEERING_VIEWS.md` + `BOARD_TERMINALS.md` |
| V8 | Exception / fault matrix, including 210° broken-bag station | `SP01_ENGINEERING_VIEWS.md` + `BROKEN_BAG_REJECT.md` |
| V9 | Supervisory/ISA-88-style projection | `SP01_ENGINEERING_VIEWS.md` |
| V10 | Memory/communication/data contracts | `SP01_ENGINEERING_VIEWS.md` + `WEIGHING.md` |
| V11 | Industrial digital-twin HMI | `SP01_ENGINEERING_VIEWS.md` + `BROKEN_BAG_REJECT.md` |
| V12 | Weighing signal quality, calibration, DSP and bounded tare | `WEIGHING_SIGNAL_QUALITY.md` |
| V13 | Eight-spout system and rotating/stationary topology | `EIGHT_SPOUT_SYSTEM_TOPOLOGY.md` + `BROKEN_BAG_REJECT.md` |

## Review decisions frozen from Purple-Team critique

```text
K-map                  REMOVE as canonical logic view
Python asyncio RT      REJECT for production controller; C++/FreeRTOS remains ground truth
dW/dt control          NOT CURRENT; future estimator must be filtered, bounded and have static fallback
Calibration/DSP        ADD as V12
Unlimited auto-tare    REJECT; any cycle tare must be bounded and separate from calibration
Broken-bag protection  DEDICATED 210° field sensor exists; weight trajectory is secondary evidence, not primary detection
Slip-ring blind spot   ADD system topology V13; do not assume data must traverse slip ring
```

## 210° broken-bag station

The installed machine has a dedicated broken-bag reject sensor at approximately 210°. This is now a physical machine fact, but its electrical ownership and reject actuator path are not yet frozen. Current SP01 local 8DI allocation has no spare channel, so no DI may be repurposed without G8 as-built evidence. See `BROKEN_BAG_REJECT.md`.

## Precedence

When views disagree:

```text
1. measured installed-machine electrical/mechanical evidence
2. executable C++ controller + board adapter
3. frozen I/O / weighing / topology contracts
4. generated state/fault/timeline views
5. supervisory/HMI projections
6. historical team proposals
```

Numeric thresholds and topology claims are frozen only from measured gate evidence.