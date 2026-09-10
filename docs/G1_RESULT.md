# G1 result — SP01 first board

**Verdict: G1 PASS** (self-test scope, defined below).

Date: 2026-09-10
Firmware: `347ec1d` (`firmware/esp32-s3/main/g1_diag.cpp`), ESP-IDF v5.5.5
Board: ESP32-S3-WROOM-1U-N16R8, MAC `d0:cf:13:23:d5:00`, silicon rev v0.2
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
| eth_link | PASS | PHYCFGR 0xBF, LNK up, 100M, full duplex, from t=2.12 s |

Stability: 31 heartbeats over 30 s, no gaps. Free heap 363944 bytes, unchanged
from first to last sample, minimum equal to current. No resets, no watchdog
events, no panics.

## What G1 does and does not establish

Established:

- the board boots and runs application code;
- the native USB Serial/JTAG console works as the sole console;
- flash size and partition layout are correct;
- the TCA9554 output expander is present, addressable, and its outputs are
  driven to the safe latch before being enabled;
- all eight digital inputs read at their unwired state;
- the SPI link to the W5500 is correctly wired and the device responds;
- the Ethernet PHY negotiates a 100M full-duplex link.

NOT established by G1:

- DI response to real field signals (needs dry contacts — G2);
- DO switching (needs the bench DO pulse test with no actuators — G2);
- RS485/TLB communication (TLB disconnected — G4);
- any weighing accuracy;
- Ethernet data path above link level — no MAC driver, no DHCP, no IP was
  exercised by this firmware;
- Wi-Fi, deliberately excluded from this build;
- long-run stability beyond 30 s.

## Findings from the bring-up itself

**1. Ethernet autonegotiation timing.** The first version of the self-test read
PHYCFGR about 100 ms after releasing the W5500 reset and reported the link as
down. Autonegotiation needs 1-3 s. The link stage now polls once per second and
latches PASS on the first LNK=1. Recorded here because a single-sample link test
produces a false negative on this board every time.

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

Each of these produced symptoms that resembled a hardware fault. None was one.

## Next gate

G2, in this order:

1. DI verification with a dry contact per channel, one at a time;
2. DO verification using the one-hot auto-off pulse test, machine actuators and
   solenoids disconnected, dummy lamp load only;
3. Ethernet data path: MAC driver, DHCP lease, ping;
4. long-run stability, several hours, with heap and reset-reason logging.

G2 is PENDING. Machine wiring stays disconnected until G2 passes.
