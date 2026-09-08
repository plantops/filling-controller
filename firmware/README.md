# SP01 Firmware v0.1

ESP32-S3 + ESP-IDF + C++17 + FreeRTOS.

Current status:

```text
M0 scaffold committed
S1 native Linux amd64 harness committed
G0 ESP-IDF build pending
```

Architecture:

```text
control    HIGH    DI image -> weight snapshot -> FSM -> interlocks -> DO image
weighing   MEDIUM  TLB485/RS485 -> latest WeightSnapshot
web        LOW     HTTP/WebSocket + static HTML/CSS/JS
storage    LOW     config + bounded event/cycle buffer
```

Hard rules:

- one control context commits physical outputs;
- control never waits for Wi-Fi, logging or a Modbus reply;
- hardware GPIO/register mapping stays outside the controller model;
- dummy 24 V I/O first;
- no raw GPIO/Modbus web writes;
- safety/E-stop remains independent.

Implementation order follows [`../docs/FW.md`](../docs/FW.md): M0 -> M9 / G0 -> G9.

## Native Linux amd64

The host harness uses the same C++ semantic model as ESP firmware.

```bash
cmake -S firmware/host -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
./build/host/sp01_host
```

Host adapters currently provide:

```text
ManualClock
VirtualIo
VirtualWeigher
```

This is the base for deterministic FSM/conformance simulation. It does not emulate ESP peripherals or the electrical behavior of the Waveshare board.

## M0 scaffold

```text
firmware/
  host/                 native Linux amd64 harness
  esp32-s3/
    CMakeLists.txt
    main/
    components/
      controller/
```

M0 contains no physical GPIO writes and no guessed machine timing constants.
