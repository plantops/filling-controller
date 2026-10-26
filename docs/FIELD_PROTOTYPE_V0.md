# SP01 Field Prototype v0 — SUPERSEDED / ARCHIVAL

Date: 2026-09-12

## Current safety status (2026-10-26)

**DO NOT FLASH OR DEPLOY THE V0 ARTIFACT LISTED BELOW FOR NEW FIELD WORK.** The V0 image was superseded after physical DI polarity mismatch and subsequent DO polarity correction. The recorded correction commits are `9ba13966337720c2ba459bc057bdee524438797a` (DI) and `7f98c76bda2878e34b3ff8705e1cf69265b3de46` (DO). The corrected field configuration sets both `CONFIG_SP01_DI_INVERT_MASK=0xFF` and `CONFIG_SP01_DO_INVERT_MASK=0xFF`. See `evidence/SP01/2026-09-13/G8/DI_POLARITY_FIX.md` and `DO_POLARITY_FIX.md`.

This document retains the original V0 procedure as historical evidence, **not a current flash instruction or G8/G9 authorization**. For another field visit, select an explicitly approved corrected image, verify its exact commit SHA and matching CI/artifact identity, and repeat local DI truth and isolated physical DO polarity/mapping/reset-safe checks. A CI PASS does not prove those physical checks. Keep all machine actuator outputs disconnected until formal G8 acceptance and locally authorized G8-to-G9 transition. Master status: issue #7 and draft PR #8.


## Frozen source and artifact

Frozen source branch:

```text
field/sp01-prototype-v0
```

Frozen source commit:

```text
24a0443580ed523049e31e140c31bd06076f1f99
```

CI workflow run:

```text
34694506046
```

Artifact:

```text
sp01-field-prototype
artifact id: 10297969116
sha256: 161463d78ff4cac8123a1c856338947b972886708f2c24683cbc4fa77b1c477e
```

Both Linux host tests and ESP32-S3 build passed for this frozen commit.

## Prototype scope

This is a field-shadow prototype, not a final qualified production build.

It intentionally uses:

```text
real DI1..DI8
Ethernet/HMI
controller FSM
synthetic/dummy weight
TLB485 disabled
normal process outputs suppressed before board commit
```

The controller still calculates desired process outputs internally, but when dummy-weight mode is enabled the firmware replaces the physical output image with the safe OFF image before writing the board. This lets the team observe DI/state/FSM behavior on the real machine without giving the new controller actuator authority.

Broken-bag thresholds remain disabled/unfrozen until measured TLB/machine evidence exists.

## Historical V0 flash procedure — NOT FOR NEW FIELD DEPLOYMENT

Historical command for reproducing the superseded V0 artifact only; **do not execute on an installed machine**. The original instruction was to unzip `sp01-field-prototype` and run:

```powershell
.\flash.ps1
```

If automatic port detection does not select the board:

```powershell
.\flash.ps1 -Port COMx
```

No local ESP-IDF environment is required by the packaged flash workflow.

## Site wiring for first shadow run

Connect:

```text
USB-C      service / flash / serial
Ethernet   HMI / telemetry
DI1..DI8   real machine input signals
```

Keep disconnected for the first field shadow:

```text
TLB485     optional; not required for dummy-weight shadow
DO1..DO8   no machine actuator authority in this build
```

Do not leave any prior G2 DO-to-DI bench loopback jumpers installed on the machine.

## Historical prototype acceptance — NOT FORMAL G8

Do not repeat exhaustive bench tests. For the first prototype visit, collect only enough evidence to decide whether the controller follows the real machine plausibly:

1. Flash succeeds and serial prints the frozen build identity.
2. Ethernet HMI is reachable.
3. Real DI states change consistently with the machine.
4. One representative cycle advances through plausible controller states.
5. `desired_do` may change with FSM state, while `commanded_do` remains physically suppressed in this shadow build.
6. Record one serial capture or HMI screenshot and any mapping/timing exception.

These checks can support only the prototype shadow/data-collection milestone. They do not establish formal G8 entry or PASS. Formal G8 requires documented G2, G2T, G3, G4, G5, G6, and G7 acceptance; G9 requires formal G8 acceptance and local authorization. Any unexpected output behavior reopens the relevant physical check.

## Deferred, not falsely passed

The following are deliberately deferred from this rapid prototype milestone:

```text
G2T full thermal/serviceability qualification
exhaustive per-channel DO proof
full TLB485 latency/noise characterization
formal zero/20/50 calibration campaign
long network-loss qualification
measured reject/normal discharge timing authority
live one-spout actuator proof
```

These remain commissioning work. Any unexpected site behavior reopens only the relevant item.

## Transition to live one-spout pilot

The shadow artifact itself must not be treated as the live-authority image because dummy-weight mode suppresses normal board outputs.

Before G9 live authority, the local team must have:

```text
real weight source selected and checked
actual machine DI mapping checked
actual reject/normal-discharge timing source identified
one-spout DO mapping confirmed locally
rollback path ready
spare controller or known-good previous controller ready
local authorization to connect the new controller outputs
```

The first live step is one spout only. If behavior diverges from the legacy sequence or any output is unexplained, disconnect new actuator authority and return to shadow/legacy control.

## Tracking

Master commissioning tracker: issue #7.
Field execution ticket: issue #9.
Long-running commissioning PR: #8.

The commissioning PR remains draft until physical field evidence exists; the frozen V0 branch is historical only and must not be selected as the current field image.
