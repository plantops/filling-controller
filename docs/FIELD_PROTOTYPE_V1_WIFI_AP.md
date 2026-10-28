# SP01 Field Prototype V1 — SUPERSEDED / ARCHIVAL

Date of safety review: 2026-10-28

> **DO NOT FLASH OR DEPLOY THE V1 IMAGE OR ARTIFACT BELOW FOR NEW FIELD WORK.**
> This V1 image predates the observed active-low DI polarity correction
> (`9ba13966337720c2ba459bc057bdee524438797a`) and DO polarity correction
> (`7f98c76bda2878e34b3ff8705e1cf69265b3de46`). The V1 source had
> `CONFIG_SP01_DI_INVERT_MASK=0x00` and `CONFIG_SP01_DO_INVERT_MASK=0x00`.
> The corrected field profile uses `0xFF` for both masks. Software CI cannot
> establish physical safe-OFF polarity or channel mapping.

This file records the historical direct Wi-Fi hotspot prototype only. It is
**not** a current flashing procedure, a field-image approval, or G8/G9 authority.
Before any new field shadow visit, select a locally approved corrected image
with exact source SHA and matching build/artifact identity. With machine
actuators **physically disconnected**, verify DI all-open OFF and each
one-contact CLOSED ON, and independently measure isolated DO safe OFF,
one-hot channel mapping, and safe OFF on boot/reset/fault. Record the actual
board identity, firmware SHA, measurements and operator. See
`docs/FIELD_PROTOTYPE_V0.md`,
`evidence/SP01/2026-09-13/G8/DI_POLARITY_FIX.md`,
`evidence/SP01/2026-09-13/G8/DO_POLARITY_FIX.md`, issue #7, field issue #9,
and `docs/GATE_EXECUTION_PLAN.md`.

Formal G8 entry requires G2, G2T, G3, G4, G5, G6 and G7 acceptance; G9
requires completed G8, red-team review, and explicit local authorization.
**Machine actuator outputs remain isolated until G8 is complete and the
documented G8-to-G9 transition is authorized locally.**

## Historical V1 purpose and artifact — NOT FOR NEW DEPLOYMENT

Purpose: allow direct HMI access at site when no Archer/router is available.

Firmware source commit: `9fb387f852256ff11ce042ebb75c1b4e38d7f7d0`.
Workflow: `fw #270`, run `34707938890` — Linux + ESP32-S3 PASS.
Artifact: `sp01-field-prototype`, id `10302292802`, sha256
`97b378b2ed6991fecd5a98be3c2248ba832b45cde89791feda1885bf17360e02`.

## Historical access paths — archival only

1. W5500 Ethernet was primary when a LAN/router was present.
2. If `CONFIG_SP01_WIFI_SSID` was empty, ESP32-S3 started a local AP:
   - SSID: `SP01-HMI`
   - Historical WPA2 password: `sp01filling` (not approved as a field credential)
   - HMI: `http://192.168.4.1`
3. If a Wi-Fi STA SSID was configured, the fallback AP was skipped.

The hotspot change did not change controller logic, dummy-weight behavior,
or shadow-output suppression. This historical V1 was a shadow image:
real DI, Ethernet/HMI or local AP, dummy WeightSnapshot, TLB485 disabled,
and normal process DO held logically OFF. Because the physical DO polarity
was subsequently found wrong, logical OFF must **not** be interpreted as
a verified physical safe-OFF output on this V1 image.
