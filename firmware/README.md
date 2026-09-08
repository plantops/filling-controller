# SP01 Firmware v0.1

ESP32-S3 + ESP-IDF + C++17 + FreeRTOS.

Current status:

```text
M0 scaffold committed
G0 clean ESP-IDF build pending
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

## M0 scaffold

```text
firmware/esp32-s3/
  CMakeLists.txt
  main/
    CMakeLists.txt
    app_main.cpp
  components/
    controller/
      CMakeLists.txt
      include/sp01/model.hpp
```

M0 intentionally contains no physical GPIO writes and no guessed timing constants. It establishes the semantic SP01 model and an ESP-IDF build target first.
