# Hardware Reference

## Project hardware philosophy

The original machine controller is obsolete/discontinued while the mechanical packer remains valuable. The open controller is therefore designed as a **replaceable service module** rather than as a permanently irreplaceable appliance.

> **DESIGN FOR REPLACEMENT, NOT IMMORTALITY.**

The low-cost ESP node may be replaced when field lifetime or temperature exposure makes that economical. The machine, load cell and weighing chain remain the protected assets.

Lifecycle/service details: [`../docs/SERVICEABILITY.md`](../docs/SERVICEABILITY.md).

## Current prototype parts

| Function | Part | Reference |
|---|---|---|
| Controller | Waveshare Industrial ESP32-S3 8DI/8DO, MLAB SKU `32108` | [MLAB local source](https://www.mlab.com.vn/industrial-esp32-s3-control-board-with-8-channel-digital-input-output-built-in-xtensa-32-bit-lx7-dual-core-processor-up-to-240mhz-with-multiple-isolation-protection-circuits) · [Waveshare wiki](https://www.waveshare.com/wiki/ESP32-S3-POE-ETH-8DI-8DO) |
| Weighing | LAUMAS `TLB485`; digital weight to ESP via isolated RS485 | [LAUMAS](https://www.laumas.com/en/product/tlb-485-digital-weight-transmitter-rs485/) |
| AP/router | TP-Link Archer C64-class | [TP-Link](https://www.tp-link.com/vn/support/download/archer-c64/v1/) |

Controller use in SP01:

```text
24 VDC terminal power
8 DI / 8 transistor DO
isolated RS485 terminal -> TLB485 -> load cell
Wi-Fi -> stationary AP/router
Ethernet/PoE unused
```

## Environmental condition

Possible machine ambient may reach approximately **70 °C**.

Do not infer a complete-system 70 °C rating from the rating of one MCU/module. G2T must characterize the real controller/TLB installation, elevated-temperature behavior, safe reset/fault response and practical replacement procedure.

Prefer the coolest practical mounting location and passive/radiant-heat mitigation before adding dust-sensitive active cooling.

If the ESP controller lifetime is economically acceptable, it may be treated as a replaceable consumable. The TLB is a separate module and should be relocated or substituted if its verified environmental capability is inadequate.

## Spare / replacement hardware

Field pilot/production should include at least one pre-flashed known-good controller spare for the packer.

The machine-side wiring should converge on labelled pluggable terminals or an adapter harness so controller replacement does not require individual field-wire re-termination.

Keep the following with the machine record rather than only inside a controller:

```text
firmware commit/tag
spout_id
I/O mapping/inversion
TLB address/profile
approved recipes
commissioned timing values
replacement history
```

Replacing only the ESP controller must not automatically change TLB calibration.

## Weighing connection rule

```text
load cell -> TLB485 -> RS485 A/B -> ESP board isolated RS485 terminal
```

Do not connect TLB485 to the 24 V DI terminals for continuous weight. Do not connect the raw load-cell bridge directly to ESP32 in production. DI1..DI8 remain machine inputs.

Firmware mapping behind the onboard RS485 transceiver is currently GPIO17 TX / GPIO18 RX / GPIO21 RTS.

Bring-up starts at 9600 bit/s / 50 ms poll; after measured G4 evidence, target 115200 bit/s / 20 ms poll. See [`../docs/WEIGHING.md`](../docs/WEIGHING.md).

MLAB price checked 2026-09-08: **1,398,000 VND ex VAT** for SKU `32108`.

Current economic assumption accepts controller replacement cost on the order of VND 1.5 million even if field life were only around six months. This is not a mandatory interval; actual field history determines maintenance policy.

## Local evidence

```text
hardware/manuals/   exact manuals/datasheets used
hardware/photos/    actual machine, wiring and as-built photos
```

Keep exact revision information with every local file. Add installation temperature measurements and replacement history once available.

BOM: [`../docs/BOM.md`](../docs/BOM.md)
