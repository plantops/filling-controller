# SP01 G2 physical dummy I/O + Ethernet — 2026-09-10

Status: **ACTIVE — 1 h soak PASS; physical DI/DO evidence still pending**

Branch: `diag/sp01-g2-g9`

## 1 h soak evidence supplied from bench

At approximately 3600 s uptime the board remained responsive over USB serial and Ethernet DHCP remained active:

```text
I (3599310) g1: STAGE eth_dhcp     PASS  ip=192.168.11.21 mac=d0:cf:13:23:d5:03
I (3599310) g1: HB 3600 up=3599140 ms heap=337316 link=UP ip=192.168.11.21
I (3599310) g1: ===== G1 SUMMARY  pass=6 fail=1 skip=0 =====
I (3599310) g1:   chip         PASS cores=2 rev=0.2 mac=d0:cf:13:23:d5:00 rst=USB
I (3599310) g1:   flash        PASS size=16384 KiB (expect 16384 for N16R8)
I (3599310) g1:   i2c_tca9554  PASS TCA9554@0x20 ok, outputs safe, INPUT=0x00
I (3599310) g1:   di_read      PASS DI1..8 raw=0xff (pulled up, unwired expects 0xFF)
I (3599310) g1:   spi_w5500    PASS VERSIONR=0x04, SPI link to W5500 confirmed
I (3599310) g1:   eth_link     FAIL PHYCFGR=0xba link=DOWN (autoneg pending, cable, or magnetics)
I (3599310) g1:   eth_dhcp     PASS ip=192.168.11.21 mac=d0:cf:13:23:d5:03
I (3599310) g1: ===== G1 VERDICT: NOT PASS =====
I (3600310) g1: STAGE eth_dhcp     PASS  ip=192.168.11.21 mac=d0:cf:13:23:d5:03
I (3600310) g1: HB 3601 up=3600141 ms heap=337316 link=UP ip=192.168.11.21
```

Capture summary supplied by operator:

```text
after: {'COM3': 'USB Serial Device (COM3)'}
bytes: 810342  verdict: DATA RECEIVED
```

## Interpretation

The 1 h soak subcriterion is accepted as PASS for this bench run:

- serial heartbeat reached 3600+ s;
- free heap at the recorded endpoint was 337316 bytes;
- Ethernet had a valid DHCP lease at `192.168.11.21`;
- heartbeat reported `link=UP` at 3600 and 3601 s;
- the USB serial device remained enumerated as COM3;
- the capture received 810342 bytes.

The summary line `eth_link FAIL PHYCFGR=0xba link=DOWN` is not treated as a sustained-link failure because the same timestamp block also records DHCP PASS, and the heartbeat immediately records `link=UP`. This is consistent with the previously identified diagnostic defect where a single early PHYCFGR sample could record autonegotiation as DOWN even though the driver subsequently obtains link and DHCP. The diagnostic summary should not be used as the sole G2 Ethernet verdict when later runtime evidence contradicts it.

## G2 remaining evidence

G2 does **not** pass yet. Still required with machine actuator wiring disconnected:

```text
DI1..DI8: OPEN -> CLOSED -> OPEN, one channel at a time, no cross-talk
DO1..DO8: dummy-load OFF -> ON pulse -> automatic OFF, one-hot, no adjacent output
reset/restart: physical outputs return to safe state
```

The sustained Ethernet/1 h soak portion is now complete for this bench session.
