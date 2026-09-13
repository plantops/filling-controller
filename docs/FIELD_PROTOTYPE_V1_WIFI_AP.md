# SP01 Field Prototype V1 — direct Wi-Fi hotspot

Purpose: allow direct HMI access at site when no Archer/router is available.

Firmware source commit: `9fb387f852256ff11ce042ebb75c1b4e38d7f7d0`.
Workflow: `fw #270`, run `34707938890` — Linux + ESP32-S3 PASS.
Artifact: `sp01-field-prototype`, id `10302292802`, sha256 `97b378b2ed6991fecd5a98be3c2248ba832b45cde89791feda1885bf17360e02`.

## Access paths

1. W5500 Ethernet remains primary when a LAN/router is present.
2. If `CONFIG_SP01_WIFI_SSID` is empty, ESP32-S3 also starts a local AP:
   - SSID: `SP01-HMI`
   - WPA2 password: `sp01filling`
   - HMI: `http://192.168.4.1`
3. If a Wi-Fi STA SSID is explicitly configured later, the fallback AP is skipped.

The hotspot change does not alter controller logic, dummy-weight behavior, or shadow-output suppression. The field prototype remains a shadow image: real DI, Ethernet/HMI or local AP, dummy WeightSnapshot, TLB485 disabled, normal process DO held safe OFF.
