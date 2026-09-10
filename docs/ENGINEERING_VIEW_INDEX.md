# SP01 Engineering View Index

Canonical engineering views for design review, commissioning, HMI and long-term maintenance.

The original Yellow-Team material is retained only as design input. Current installed-machine evidence, executable firmware and frozen interface contracts take precedence.

| View | Canonical purpose | Source |
|---|---|---|
| V1 | Runtime timeline and event evidence | `SP01_ENGINEERING_VIEWS.md` + `BROKEN_BAG_REJECT.md` |
| V2 | State logic matrix | `SP01_ENGINEERING_VIEWS.md` + `BROKEN_BAG_REJECT.md` |
| V3 | Interlock/process flow | `SP01_ENGINEERING_VIEWS.md` + `BROKEN_BAG_REJECT.md` |
| V4 | Interlock equations; K-map non-canonical | `SP01_ENGINEERING_VIEWS.md` + `BROKEN_BAG_REJECT.md` |
| V5 | Logic dependency graph | `SP01_ENGINEERING_VIEWS.md` |
| V6 | Executable C++ engine / ground truth | controller source + `SP01_ENGINEERING_VIEWS.md` |
| V7 | Physical I/O / terminals | `BOARD_TERMINALS.md` + `SP01_ENGINEERING_VIEWS.md` |
| V8 | Exception / fault / controlled-reject matrix | `SP01_ENGINEERING_VIEWS.md` + `BROKEN_BAG_REJECT.md` |
| V9 | Supervisory/ISA-88-style projection | `SP01_ENGINEERING_VIEWS.md` |
| V10 | Memory/communication/data contracts | `WEIGHING.md` + `SP01_ENGINEERING_VIEWS.md` |
| V11 | Industrial digital-twin HMI | `SP01_ENGINEERING_VIEWS.md` + `BROKEN_BAG_REJECT.md` |
| V12 | Weighing signal quality, calibration, DSP and bounded tare | `WEIGHING_SIGNAL_QUALITY.md` + `BROKEN_BAG_REJECT.md` |
| V13 | Eight-spout rotating/stationary topology | `EIGHT_SPOUT_SYSTEM_TOPOLOGY.md` + `BROKEN_BAG_REJECT.md` |

## Frozen process facts relevant to the views

```text
Production controller      ESP32-S3 / ESP-IDF / C++ / FreeRTOS
Weight source              LAUMAS TLB485 over isolated RS485
Broken-bag detection       weight falls while filling because loss > incoming fill
Detection implementation   bounded filtered finite-window delta; thresholds from G4/G8 evidence
On broken-bag decision     latch REJECT and immediately remove DO4..DO8
Reject eject               one bag.push near ~210°
Normal healthy eject       one bag.push near ~355°
Rejected cycle             must not later push at ~355°
210°                       reject/eject position, not a broken-bag sensor
```

The immediate reject shutdown currently means:

```text
DO4 dosing.valve_a OFF
DO5 dosing.valve_b OFF
DO6 dosing.valve_c OFF
DO7 filling.motor  OFF
DO8 spout.aeration OFF
```

Do not infer additional `scanner.down` or `bag_detect_air` behavior without machine evidence.

## How the standard views must represent broken-bag handling

```text
V1  show weight loss -> reject decision -> immediate fill shutdown -> push@210 -> suppress@355
V2  show a controlled REJECT branch from COARSE_FILL/FINE_FILL
V3  show detector -> latch -> fill OFF -> wait 210 -> push -> complete/reject outcome
V4  keep detector as a bounded predicate; no K-map
V8  distinguish controlled REJECT from controller/weight/I-O faults
V11 show detector evidence, disposition, desired/physical DO and selected eject window
V12 own filtering/window/noise/threshold evidence
V13 keep reject disposition bound to the correct spout_id + cycle_id while rotating
```

## Review decisions retained from Purple-Team critique

```text
K-map                  REMOVE as canonical logic view
Python asyncio RT      REJECT for production control
raw HX711 path         REJECT for production; TLB485 remains weighing boundary
dW/dt point trip       REJECT; use bounded filtered finite-window detection
Calibration/DSP        V12
Unlimited auto-tare    REJECT; any cycle tare must be bounded and separate from calibration
Broken-bag behavior    DETECT from weight loss during fill; immediately stop filling; reject at ~210°
Slip-ring/system view  V13; do not invent a data path before as-built evidence
```

## Precedence

When views disagree:

```text
1. measured installed-machine electrical/mechanical/process evidence
2. executable C++ controller + board adapter
3. frozen I/O / weighing / reject / topology contracts
4. generated state/fault/timeline views
5. supervisory/HMI projections
6. historical team proposals
```

A known installed-machine behavior may therefore expose a software gap. In that case the view records the required behavior and G3 stays open until V6 catches up.

Numeric detector thresholds, angular windows and actuator-lead values are frozen only from gate evidence.