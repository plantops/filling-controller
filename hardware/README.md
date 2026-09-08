# SP01 Hardware Reference Index

This directory is the entry point for the physical SP01 v0.1 prototype. It complements the language-neutral documents under `docs/`.

## Selected prototype components

| Function | Prototype selection | Official reference |
|---|---|---|
| Controller / local I/O | Waveshare `ESP32-S3-POE-ETH-8DI-8DO` | https://www.waveshare.com/esp32-s3-poe-eth-8di-8do.htm |
| Controller documentation | Waveshare wiki | https://www.waveshare.com/wiki/ESP32-S3-POE-ETH-8DI-8DO |
| Weighing transmitter | LAUMAS `TLB485` | https://www.laumas.com/en/product/tlb-485-digital-weight-transmitter-rs485/ |
| Stationary AP/router candidate | TP-Link Archer C64-class | https://www.tp-link.com/vn/support/download/archer-c64/v1/ |

The controller carrier is a **prototype carrier**, not a permanent brand dependency. Production acceptance requires electrical/environmental evidence from the actual machine.

## Prototype topology

```text
STATIONARY
Laptop --LAN/Wi-Fi--> dedicated AP/router
                           )))
                           ))) supervisory only

ROTATING SP01
ESP32-S3
  |-- DI01..DI08 <- dummy 24 V switches first
  |-- DO01..DO08 -> dummy loads first
  |-- RS485 -> TLB485 -> load cell
  `-- Wi-Fi STA
```

No Ethernet data path crosses the rotor in v0.1.

## BOM

Authoritative procurement list:

- [`../docs/BOM_SP01_V01.md`](../docs/BOM_SP01_V01.md)

Purchase one SP01 set only. Do not multiply by eight until the pilot gates pass.

## Weighing calibration

Authoritative workflow:

- [`../docs/WEIGHING_CALIBRATION.md`](../docs/WEIGHING_CALIBRATION.md)

Calibration is performed through the firmware/HMI service layer and TLB adapter, not direct browser access to Modbus registers.

## Manuals

See [`manuals/README.md`](manuals/README.md).

Prefer official vendor URLs. Store a local PDF only when it is provided for the project and redistribution/storage is permitted. Record the exact hardware revision that each manual applies to.

## Photos

See [`photos/README.md`](photos/README.md).

Photos of the actual packer, SP01 wiring, controller carrier, TLB terminals, antenna location and final as-built panel are part of the commissioning evidence. Vendor product photos are reference-only and should normally remain external links.

## As-built evidence to capture

For every purchased/installed item record:

```text
manufacturer
exact part number
hardware revision
serial number if applicable
supplier
purchase date
firmware version
manual revision
terminal/wiring photo
measured supply voltage/current
measured output load/current
commissioning notes
```

The BOM starts as a procurement list and ends as the SP01 as-built hardware record.
