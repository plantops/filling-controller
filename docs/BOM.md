# SP01 BOM v0.1

One bench prototype plus field-service items needed to validate the replaceable-controller strategy.

| Item | Qty | Selected / requirement | Source |
|---|---:|---|---|
| Controller | 1 | Waveshare Industrial ESP32-S3 8DI/8DO, MLAB SKU `32108`; power from 24 VDC terminal; Ethernet/PoE unused | [MLAB](https://www.mlab.com.vn/industrial-esp32-s3-control-board-with-8-channel-digital-input-output-built-in-xtensa-32-bit-lx7-dual-core-processor-up-to-240mhz-with-multiple-isolation-protection-circuits) |
| Pre-flashed controller spare | 1 for field pilot/production | same approved controller/image; bench-verified safe before storage | |
| Weighing transmitter | 1 | LAUMAS `TLB485`; production weight source to controller via RS485 | [LAUMAS](https://www.laumas.com/en/product/tlb-485-digital-weight-transmitter-rs485/) |
| Wi-Fi AP/router | 1 | dedicated 2.4 GHz; Archer C64-class candidate | [TP-Link](https://www.tp-link.com/vn/support/download/archer-c64/v1/) |
| External Wi-Fi antenna | 1 + spare | 2.4 GHz, correct connector/pigtail | |
| USB-RS485 adapter | 1 | TLB commissioning / independent diagnostics | |
| Shielded twisted-pair RS485 cable | 5–10 m | TLB485 A/B ↔ ESP board isolated RS485 terminal | |
| RS485 termination/bias parts | as required | use only per verified TLB + board topology | |
| DIN rail/enclosure/backplate | 1 set | bench then SP01; choose serviceable mounting with thermal/radiant-heat consideration | |
| Pluggable/keyed harness or terminal adapter | 1 set | minimize field re-termination during controller swap | |
| Fused terminals/fuses/MCB | 1 set + service spares | 24 V wiring | |
| Wire/ferrules/labels/glands | 1 set | panel wiring and clear replacement identification | |
| 24 V DI switches | 8 | dummy machine inputs; no DI is allocated to continuous weight | |
| 24 V lamps/dummy loads | 8 | dummy outputs | |
| Spare 24 V solenoid/inductive loads | several | output stress test and RS485-noise test | |
| Temperature probes/logger | 1 set for G2T | measure actual controller/TLB ambient and elevated-temperature bench behavior | |
| Passive heat/radiant shield material | as required after measurement | use only if field survey shows benefit | |
| USB programming/debug cable | 2 | firmware/service | |
| Printed/portable recovery sheet | 1 per machine | version/tag, wiring, TLB address, approved config and swap procedure | |

MLAB listed price checked 2026-09-08: **1,398,000 VND, ex VAT** for SKU `32108`.

Current economic design assumption: a controller cost of roughly **VND 1.5 million** is cheap enough that replacement even on the order of six months can be acceptable compared with packer downtime or loss of the mechanical asset. This is not a mandatory replacement interval; field lifetime will be measured.

Existing 24 VDC, load cell and machine actuators are verified before field connection.

## Weighing boundary

```text
load cell -> TLB485 -> isolated RS485 -> ESP32
```

Do not add a raw load-cell ADC path to ESP32 for production and do not consume a DI for continuous weight. See [`WEIGHING.md`](WEIGHING.md).

The TLB is a separate service module from the low-cost ESP controller. Replacing the ESP should not disturb TLB calibration. If the installed TLB cannot operate acceptably at the measured machine temperature, relocate it or select an appropriate transmitter rather than treating it automatically as the same consumable class as the ESP node.

## Serviceability rule

Field deployment should make the controller a replaceable module rather than a rewiring task. A known-good spare must carry a known firmware version and recoverable as-built configuration.

See [`SERVICEABILITY.md`](SERVICEABILITY.md).

## As-built record

For purchased hardware record exact part number, revision, serial number, supplier, installed location and photo in `hardware/`.

Also record:

```text
controller firmware commit/tag
spout_id
TLB serial/address/profile
installed temperature measurements
approved target recipes
commissioned timing values
replacement history
```
