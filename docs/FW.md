# SP01 Firmware v0.1

Target stack:

```text
ESP32-S3
ESP-IDF
C++
FreeRTOS
```

Python remains on `py-sim` as the simulator/reference.

## Control loop

```text
read/freeze DI image
-> read latest validated weight snapshot
-> run FSM
-> build desired DO image
-> apply interlocks
-> commit DO image
```

Only one control context writes physical outputs.

TLB/RS485, Wi-Fi/HMI and logging run outside the control path. Control never waits for them.

## First implementation order

```text
F0 board/toolchain bring-up
F1 safe boot/reset outputs with dummy loads
F2 DI process image
F3 DO process image
F4 monotonic FSM + state timeouts + watchdog
F5 TLB485 Modbus adapter
F6 weight age/quality + stale abort
F7 calibration service + UI
F8 local cycle/event buffer
F9 Wi-Fi STA + small web HMI
F10 bench stress and timing evidence
```

## HMI v0.1

Keep it small:

```text
Status     state, weight, target, faults
I/O        DI1..DI8, DO1..DO8
Timing     state/I-O/weight timeline
Calibration
Diagnostics TLB + Wi-Fi + firmware
```

No raw GPIO or raw Modbus write endpoint.

## Test gates

```text
dummy I/O
-> TLB/calibration
-> dry cycles
-> AP/Wi-Fi failure tests
-> rotating RF + shadow
-> review
-> controlled field output
```
