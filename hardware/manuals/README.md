# Hardware Manuals

Keep this directory as the index for manuals that directly apply to the SP01 prototype.

## Official sources

### Waveshare ESP32-S3-POE-ETH-8DI-8DO

- Product: https://www.waveshare.com/esp32-s3-poe-eth-8di-8do.htm
- Wiki/documentation: https://www.waveshare.com/wiki/ESP32-S3-POE-ETH-8DI-8DO

Before wiring real loads, retain the exact documentation/revision covering DI thresholds, DO topology/current limits, RS485 isolation/termination, power input, antenna connector and boot/reset output behavior.

### LAUMAS TLB485

- Product/download page: https://www.laumas.com/en/product/tlb-485-digital-weight-transmitter-rs485/

The official page provides the data sheet and identifies TLB485 manuals/protocol manuals; some manual downloads may require a LAUMAS login. Retain the exact manual and protocol revision used for firmware development and calibration commissioning.

Required information from the manual/protocol set:

```text
load-cell terminal wiring
excitation / sense wiring
calibration commands/procedure
weight/status register or command definitions
RS485 settings
Modbus RTU framing
filtering/update behavior
zero/tare behavior
fault/status codes
nonvolatile configuration behavior
```

### Prototype AP/router

- TP-Link Archer C64 support/download page: https://www.tp-link.com/vn/support/download/archer-c64/v1/

The router is supervisory infrastructure only. Record the actual hardware revision because firmware/manuals differ by revision.

## Local PDF policy

Do not copy vendor PDFs into this repository merely for convenience. Add a local PDF when one of these is true:

1. the user/project owns or is permitted to retain it;
2. it is required for offline commissioning and its license permits storage;
3. it is an internally created wiring/calibration/as-built manual.

Use descriptive filenames, for example:

```text
waveshare-esp32-s3-8di8do-hw-revX-manual.pdf
laumas-tlb485-user-manual-revX.pdf
laumas-tlb-protocols-revX.pdf
sp01-as-built-wiring-v0.1.pdf
```

When a PDF is added, update this index with document revision, source and applicable hardware revision.
