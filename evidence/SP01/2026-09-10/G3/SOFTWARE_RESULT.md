# SP01 G3 software prequalification — 2026-09-10

**Verdict: SOFTWARE PREQUALIFICATION PASS. Full G3 remains PENDING board/dummy-I/O evidence.**

Branch: `diag/sp01-g2-g9`
Baseline diagnostic head: `c8bbfa8a0a92f0d899497aa4d8b2e41647b9811c`
Test-wiring commit: `030807615d35408ff0cc3d50187acc459efa9847`
CI run: `34472688312` / workflow run #117

## CI evidence

- Linux host configure/build: PASS.
- Existing controller tests: PASS.
- Expanded `sp01_g3_gate_tests`: PASS through CTest.
- Host smoke: PASS.
- ESP32-S3 build under ESP-IDF v5.5.5: PASS.
- ESP32 bundle assembly/upload: PASS.

## Logic covered

Existing suite:

- normal AUTO full cycle;
- discharge countdown scales with measured A->B interval;
- MANUAL full fill without automatic bag push;
- MANUAL OFF during filling returns to safe idle;
- mode change during active filling faults with outputs OFF.

Expanded G3 matrix:

- bag acquire timeout -> `BAG_MISSING`, outputs OFF;
- permissive loss -> `PERMISSIVE_LOST`, outputs OFF;
- bag lost -> `BAG_LOST`, outputs OFF;
- stale weight at tare/coarse/fine/settle -> `WEIGHT_STALE`, outputs OFF;
- transmitter fault during fill -> `WEIGHT_FAULT`, outputs OFF;
- coarse timeout -> `STATE_TIMEOUT`, outputs OFF;
- fine timeout -> `STATE_TIMEOUT`, outputs OFF;
- discharge-reference timeout -> `STATE_TIMEOUT`, outputs OFF;
- B-before-A discharge timing -> `DISCHARGE_TIMING_INVALID`, outputs OFF;
- forced `IO_FAULT` -> outputs OFF;
- reset -> `WAIT_PERMISSIVE` with outputs OFF;
- fault clear rejected while process initiative is ON; accepted after initiative OFF.

## What this does not prove

This result is controller-software evidence only. It does not prove physical DI polarity, physical DO switching, TCA9554 behavior under a commanded pulse, board timing under field wiring, RS485/TLB behavior, thermal behavior, or machine operation.

Full G3 closes only after the same dry-cycle behavior is observed on the ESP32 diagnostic/bench image with gate evidence recorded. `tools/vbench_gate.py` is provided to make that run repeatable over the virtual bench; physical dummy-I/O evidence remains separate under G2.
