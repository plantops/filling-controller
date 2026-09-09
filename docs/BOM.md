# SP01 BOM v0.1

One bench prototype.

| Item | Qty | Selected / requirement | Source |
|---|---:|---|---|
| Controller | 1 | Waveshare Industrial ESP32-S3 8DI/8DO, MLAB SKU `32108`; power from 24 VDC terminal; Ethernet/PoE unused | [MLAB](https://www.mlab.com.vn/industrial-esp32-s3-control-board-with-8-channel-digital-input-output-built-in-xtensa-32-bit-lx7-dual-core-processor-up-to-240mhz-with-multiple-isolation-protection-circuits) |
| Weighing transmitter | 1 | LAUMAS `TLB485`; production weight source to controller via RS485 | [LAUMAS](https://www.laumas.com/en/product/tlb-485-digital-weight-transmitter-rs485/) |
| Wi-Fi AP/router | 1 | dedicated 2.4 GHz; Archer C64-class candidate | [TP-Link](https://www.tp-link.com/vn/support/download/archer-c64/v1/) |
| External Wi-Fi antenna | 1 + spare | 2.4 GHz, correct connector/pigtail | |
| USB-RS485 adapter | 1 | TLB commissioning / independent diagnostics | |
| Shielded twisted-pair RS485 cable | 5–10 m | TLB485 A/B ↔ ESP board isolated RS485 terminal | |
| RS485 termination/bias parts | as required | use only per verified TLB + board topology | |
| DIN rail/enclosure/backplate | 1 set | bench then SP01 | |
| Fused terminals/fuses/MCB | 1 set | 24 V wiring | |
| Wire/ferrules/labels/glands | 1 set | panel wiring | |
| 24 V DI switches | 8 | dummy machine inputs; no DI is allocated to continuous weight | |
| 24 V lamps/dummy loads | 8 | dummy outputs | |
| Spare 24 V solenoid/inductive loads | several | output stress test and RS485-noise test | |
| USB programming/debug cable | 2 | firmware/service | |

MLAB listed price checked 2026-09-08: **1,398,000 VND, ex VAT** for SKU `32108`.

Existing 24 VDC, load cell and machine actuators are verified before field connection.

## Weighing boundary

```text
load cell -> TLB485 -> isolated RS485 -> ESP32
```

Do not add a raw load-cell ADC path to ESP32 for production and do not consume a DI for continuous weight. See [`WEIGHING.md`](WEIGHING.md).

## As-built record

For purchased hardware record exact part number, revision, serial number, supplier and photo in `hardware/`.
