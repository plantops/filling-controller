# SP01 G7 — machine-contract divergence (static review; not gate evidence)

Date: 2026-10-30
Commissioning branch: `diag/sp01-g2-g9` at `9dea3d210a68207ceba5f0c80900f185b41eeb11`
Independent CP design branch: `feat/sp01-cp-core` at `dc85cb3c0a2dc2750f7b4c1c4ceb6cdec6b5109b`
Status: **OPEN DESIGN / MACHINE-IDENTITY RECONCILIATION; NO GATE ADVANCE**

## Source-grounded observation

The commissioning branch's `docs/BOARD_TERMINALS.md`, `docs/SP01_CANONICAL_VIEWS.md`, and `firmware/esp32-s3/components/controller/include/sp01/model.hpp` define a legacy-style I/O and process contract. Examples:

| Terminal / behavior | Commissioning branch | Separate CP design branch |
| --- | --- | --- |
| DI1 | hopper feeder running | bag.present (PE pressure converter) |
| DI2 | downstream conveyor ready | prox.start (one pulse per revolution) |
| DI3 | machine motor running / AUTO mode | prox.discharge (mechanical downstream interlock) |
| DO1 | scanner.down | feed.main (coarse gate position) |
| DO2 | bag-detect air | feed.dribble (fine gate position) |
| DO3 | bag.push | bag.holder (bag clamp) |
| DO4 | dosing valve A | bag.discharge |
| Position model | separate fill position and discharge refs A/B, proposed reject ~210 and GOOD ~355 | two proximity inputs per spout, start-index interpolation for monitoring and a mechanical discharge-permission sensor |
| Broken bag | qualified running-high-water measured-weight loss | CP source describes bag-pressure and low filling-rate behavior; actual thresholds are not yet measured |

The CP branch's `docs/SP01_MACHINE_CP.md` describes a Claudius Peters R8 ZML and PACTRON DIALOG 165B replacement, citing an OEM manual and user confirmations. It states the five-sensor model belongs to HAVER, not CP. Its independent `sp01::cp` controller and `io_map.hpp` are **not present** on the commissioning branch. GitHub compare `diag/sp01-g2-g9...feat/sp01-cp-core` shows the branches diverged; the CP branch is one commit ahead and 39 behind the commissioning branch as inspected.

These are **incompatible semantic terminal maps**. A software build or G3 test PASS against the commissioning branch's existing map does not establish that map is correct for the installed CP machine. This review does not independently verify the installed machine nameplate, as-built wiring, actual DO polarity/load, or field sequence. It does not retroactively change recorded gate statuses or claim a physical failure.

## Required decision / local evidence

1. **With all SP01 actuator outputs physically isolated**, identify the actual target spout/machine: nameplate, existing PACTRON controller identity, as-built terminal diagram and photos. Compare the actual DI/DO wires, sensor types, gate-cylinder arrangement, bag-detect method, discharge interlock and emergency-stop circuit against both source contracts.
2. Project owner/operator must select and sign off the authoritative machine contract and an approved one-spout terminal mapping. Do not copy either source map directly to actuator wiring without this physical reconciliation.
3. If CP is confirmed, reconcile/port the CP controller, board adapter, tests, canonical views, and gate plan onto the commissioning branch under review. Re-run G3 against the accepted CP machine contract and repeat G7 R1 review. Do not treat old G3 test evidence as CP-specific proof.
4. Continue physical G2T/G4/G5/G6 acceptance separately. Formal G8 requires all documented prerequisites and real machine shadow evidence; G9 requires formal G8 acceptance and explicit local authority.

## CI and authority boundary

No CI or firmware build was run for this document-only finding. At inspection the commissioning HEAD had zero check runs and statuses; one Claude check suite was queued without conclusion. Direct `git ls-remote` failed DNS resolution in the execution environment. No machine I/O or firmware code is changed by this note.

**No gate advanced. No machine actuator authority.** Keep actuator outputs physically isolated until formal G8 acceptance and locally authorized G8-to-G9 transition.
