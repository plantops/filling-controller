# SP01 Field Prototype v0

Date: 2026-09-12

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

## Fast flash

Download and unzip `sp01-field-prototype`, then from PowerShell in the extracted directory:

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

## Minimum field acceptance

Do not repeat exhaustive bench tests. For the first prototype visit, collect only enough evidence to decide whether the controller follows the real machine plausibly:

1. Flash succeeds and serial prints the frozen build identity.
2. Ethernet HMI is reachable.
3. Real DI states change consistently with the machine.
4. One representative cycle advances through plausible controller states.
5. `desired_do` may change with FSM state, while `commanded_do` remains physically suppressed in this shadow build.
6. Record one serial capture or HMI screenshot and any mapping/timing exception.

If these six checks are satisfactory, treat the field-shadow objective as achieved and move directly to integration of real weight/TLB and then locally authorized one-spout live authority. Do not spend time reopening waived G2 exhaustive channel tests unless field behavior is inconsistent.

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

The commissioning PR remains draft until physical field evidence exists; the frozen prototype branch is the source-of-truth for this first site image.
