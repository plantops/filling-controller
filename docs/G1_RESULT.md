# G1 result — SP01 first board

**Verdict: G1 PASS** (self-test scope, defined below).

Date: 2026-09-10
Firmware: `82aa361e` (`firmware/esp32-s3/main/g1_diag.cpp`), ESP-IDF v5.5.5
Board: ESP32-S3-WROOM-1U-N16R8, silicon rev v0.2
Base MAC: `d0:cf:13:23:d5:00` — Ethernet MAC `d0:cf:13:23:d5:03` (base + 3)
Method: remote, native USB Serial/JTAG only, `tools/g1_usb_probe.py`
Machine wiring: disconnected throughout

## Stage results

| Stage | Verdict | Evidence |
|---|---|---|
| chip | PASS | 2 cores, rev 0.2, MAC read, reset reason reported |
| flash | PASS | 16384 KiB, matches N16R8 |
| i2c_tca9554 | PASS | probe at 0x20 ok, safe latch written before outputs enabled, readback ok |
| di_read | PASS | DI1..8 raw 0xFF, consistent with internal pull-ups and no wiring |
| spi_w5500 | PASS | VERSIONR = 0x04 |
| eth_link | PASS | 100M full duplex; PHYCFGR 0xBF pre-driver, `ETHERNET_EVENT_CONNECTED` after |
| eth_dhcp | PASS | lease 192.168.11.21/24, gw 192.168.11.1, hostname `sp01-g1` |

Bring-up timeline, measured from reset:

```text
0.310 s   esp_eth driver started, netif attached
2.310 s   ETHERNET_EVENT_CONNECTED
5.490 s   DHCP lease acquired
```

Stability over the 30 s capture: 31 heartbeats, no gaps, no resets, no watchdog
events, no panics. Free heap 337296 bytes, unchanged from the first post-DHCP
sample to the last. The step down from 363944 bytes reflects lwIP and the
Ethernet driver allocating at startup.

## What G1 does and does not establish

Established:

- the board boots and runs application code;
- the native USB Serial/JTAG console works as the sole console;
- flash size and partition layout are correct;
- the TCA9554 output expander is present, addressable, and its outputs are
  driven to the safe latch before being enabled;
- all eight digital inputs read at their unwired state;
- the SPI link to the W5500 is correctly wired and the device responds;
- the Ethernet PHY negotiates 100M full duplex;
- the full network stack reaches DHCP: MAC driver, netif, lease, gateway.

NOT established by G1:

- DI response to real field signals (needs dry contacts — G2);
- DO switching (needs the bench DO pulse test with no actuators — G2);
- RS485/TLB communication (TLB disconnected — G4);
- any weighing accuracy;
- sustained network throughput or behaviour under load;
- Wi-Fi, deliberately excluded from this build;
- stability beyond 30 s — see the soak procedure below, which is required
  before G2.

## Soak procedure (required before G2)

Thirty seconds of uptime proves startup, not stability. A W5500 on SPI with an
interrupt-driven MAC is a candidate for behaviour that only appears after an
hour. Run at least one hour, preferably overnight:

```powershell
python tools\g1_usb_probe.py --port COM3 --seconds 3600 --log soak.txt
```

Read the result for:

| Symptom in `soak.txt` | Meaning |
|---|---|
| `heap=` declining across the run | leak in the driver or event path |
| gap in `HB n` numbering | reset; check the next `reset=` line |
| `reset=` anything but `USB` or `POWERON` | panic, watchdog, or brownout |
| `ETH EVENT: link disconnected` | link flap; note whether it recovers |
| `ip=0.0.0.0` returning mid-run | lease lost or renewal failure |
| heartbeat stops entirely | hang; pull the coredump partition |

A clean hour with flat heap and no events is the pass condition.

Assign a static DHCP reservation for `d0:cf:13:23:d5:03` before building
anything that depends on the address.

## Findings from the bring-up itself

**1. Ethernet autonegotiation timing.** The first self-test read PHYCFGR about
100 ms after releasing the W5500 reset and reported the link down.
Autonegotiation needs 1-3 s; the link came up at 2.12 s once polled per second.
A single-sample link test produces a false negative on this board every time.

**2. `write_flash @flash_args` writes nothing on PowerShell.** `@` is
PowerShell's splatting operator; unquoted, the argument is dropped and esptool
reports success having written nothing. Three separate images were believed
flashed and none were. See `docs/ONBOARDING.md` section 8.2.

**3. A returning `app_main()` is indistinguishable from a dead board on this
console.** The IDF USB Serial/JTAG driver discards output when no host has the
port open, so a firmware that prints once at boot and returns shows an empty
terminal forever afterwards. Diagnostic builds must heartbeat and must not
return.

**4. CI produced no artifact for the branch under test.** Both upload steps were
gated on `github.event_name != 'pull_request'` while the diagnostic branch was
not in the `push` trigger list, so every run uploaded nothing. Fixed.

**5. Physical link is not network reachability.** A lit LINK LED and a negotiated
PHY produce no IP address on their own. The build that passed `eth_link` had no
MAC driver, no netif and no DHCP client, so the router had nothing to show. Link
state and stack state are separate claims and are now reported separately.

**6. Stage verdicts must track their evidence source.** After `esp_eth` takes
ownership of the SPI device, the raw PHYCFGR read behind the `eth_link` stage is
no longer valid, and the stage held a stale boot-time FAIL while the driver
reported the link up. Stage 5 now follows the driver's own link events once the
stack is running.

Findings 1 through 4 each produced symptoms that resembled a hardware fault.
None was one.

## Next gate

G2, in this order:

1. soak run above, one hour minimum, clean;
2. DI verification with a dry contact per channel, one at a time;
3. DO verification using the one-hot auto-off pulse test, machine actuators and
   solenoids disconnected, dummy lamp load only;
4. network data path under load: sustained traffic, not only a lease.

G2 is PENDING. Machine wiring stays disconnected until G2 passes.
