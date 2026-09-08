# Hardware Reference

## Current prototype parts

| Function | Part | Reference |
|---|---|---|
| Controller | Waveshare Industrial ESP32-S3 8DI/8DO, MLAB SKU `32108` | [MLAB local source](https://www.mlab.com.vn/industrial-esp32-s3-control-board-with-8-channel-digital-input-output-built-in-xtensa-32-bit-lx7-dual-core-processor-up-to-240mhz-with-multiple-isolation-protection-circuits) · [Waveshare wiki](https://www.waveshare.com/wiki/ESP32-S3-POE-ETH-8DI-8DO) |
| Weighing | LAUMAS `TLB485` | [LAUMAS](https://www.laumas.com/en/product/tlb-485-digital-weight-transmitter-rs485/) |
| AP/router | TP-Link Archer C64-class | [TP-Link](https://www.tp-link.com/vn/support/download/archer-c64/v1/) |

Controller use in SP01:

```text
24 VDC terminal power
8 DI / 8 transistor DO
isolated RS485 -> TLB485
Wi-Fi -> stationary AP/router
Ethernet/PoE unused
```

MLAB price checked 2026-09-08: **1,398,000 VND ex VAT** for SKU `32108`.

## Local evidence

```text
hardware/manuals/   exact manuals/datasheets used
hardware/photos/    actual machine, wiring and as-built photos
```

Keep exact revision information with every local file.

BOM: [`../docs/BOM.md`](../docs/BOM.md)
