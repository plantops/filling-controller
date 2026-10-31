# G1 remote bring-up over USB Serial/JTAG

For a board reachable only through its native USB port, with no UART adapter,
no scope and no logic analyser.

Machine wiring stays disconnected for all of G1.

---

## 0. The rule that comes before everything

**Read the running `App version` before interpreting any test result.**

A flash that reports success is not evidence that the application partition
changed. On 2026-09-10 a full day of "silent console" diagnosis was spent on a
board that had been running the same image, `384a959`, the whole time. Three
newer images were believed to be flashed. None of them were on the chip.

The boot log prints the truth:

```text
I (212) app_init: App version:      384a959
I (212) app_init: Compile time:     Sep  9 2026 16:23:57
```

Compare that string against the `GIT_SHA` file in the flash bundle. If they
differ, stop and fix the flash. Nothing downstream means anything until they
match.

---

## 1. Two properties of this console that produce false "dead board" readings

**1. Output is discarded when no host has the port open.** The IDF USB
Serial/JTAG driver drops writes when the CDC port is unopened. A terminal
started after boot has already missed the entire boot log, permanently.

**2. Firmware whose `app_main()` returns prints nothing afterwards.** The
`384a959` image finishes its startup sequence at 328 ms and then prints nothing
for the rest of time. Connecting at any later moment shows an empty terminal.

Combined, these look exactly like a board that never boots. They are not.
`tools/g1_usb_probe.py` exists to remove both effects: it opens the port first,
then resets.

A diagnostic build must never let `app_main()` return. Use a heartbeat task plus
an infinite loop, as `main/g1_diag.cpp` does.

---

## 2. Capture the boot log

```powershell
python -m pip install --user pyserial esptool
python tools\g1_usb_probe.py --port COM3 --log g1_probe_log.txt
```

Let it run the full window. Do not Ctrl+C. If nothing appears in the first few
seconds, press the board RST/EN button while the script is still running — the
port stays open across a physical reset.

Verdicts:

| Verdict | Meaning |
|---|---|
| `DATA RECEIVED` | CPU executing. Read the identity block. |
| `SILENT, PORT ALIVE` | No output captured. Try `--reset dtr`, or a manual RST press. |
| `PORT DISAPPEARED` | The app re-enumerated USB. Firmware is alive; the old terminal handle was dead. |

Close any other program holding the port first — Windows grants exclusive
access, and one stale PuTTY or VS Code serial monitor blocks everything.

---

## 3. Confirm what is on the flash

```powershell
esptool --chip esp32s3 -p COM3 read_flash 0x0     0x8000  boot_readback.bin
esptool --chip esp32s3 -p COM3 read_flash 0x8000  0xC00   pt_readback.bin
esptool --chip esp32s3 -p COM3 read_flash 0x10000 0x40000 app_readback.bin

esptool image_info --version 2 app_readback.bin
```

Check chip ID reads ESP32-S3, magic byte is `0xE9`, and the SHA256 matches the
app `.bin` in the bundle you believe you flashed.

---

## 4. Flash, then verify identity

Flash from inside the bundle directory so the relative paths in `flash_args`
resolve:

```powershell
cd dist\esp32-s3
esptool --chip esp32s3 -p COM3 write_flash @flash_args
cd ..\..
python tools\g1_usb_probe.py --port COM3
```

Then read the `App version` line and compare it with `dist\esp32-s3\GIT_SHA`.
This step is not optional. It is the check that would have caught the 2026-09-10
failure in under a minute.

---

## 5. CI facts an operator needs

Two properties of `.github/workflows/fw-host.yml` as of this writing:

1. Both upload steps are gated `if: github.event_name != 'pull_request'`, and
   the `push` trigger lists only `[main, fw-sp01-v0.1]`. A diagnostic branch
   such as `diag/sp01-g1-minimal` therefore produces **no artifact** from
   PR-triggered runs. Use `workflow_dispatch`, or add the branch to the push
   list.
2. A failed `esp32-s3` job skips the bundle steps silently. Run #77
   (`01f978ff`) failed to build, so no image for that commit exists. Check the
   run conclusion before looking for its artifact.

Download artifacts only by run, and read the `GIT_SHA` file inside before
flashing.

---

## 6. Reading the `384a959` boot log

Recorded 2026-09-10 for reference. Every line below is expected behaviour for
that image with machine wiring disconnected.

```text
boot: Loaded app from partition at offset 0x10000   partition layout is correct
spi_flash: flash io: dio                            flash mode in use
board_io: 8DO TCA9554@0x20; outputs safe            I2C expander present, outputs safe
tlb485: RS485 UART1 GPIO17/18 RTS21 9600 8N1        RS485 configured
W tlb485: TLB metadata not available: TIMEOUT       expected, TLB disconnected
W web_hmi: Wi-Fi SSID empty; HMI disabled           expected, SSID unset in this build
sp01: WAIT_PERMISSIVE -> WAIT_FILL_POSITION         controller FSM running
main_task: Returned from app_main()                 nothing prints after this
```

No Ethernet driver appears in this log — `384a959` does not initialise the
W5500. A lit LAN LINK LED with no DHCP and no MAC on the wire is therefore the
expected result for this image, not a fault. Do not diagnose W5500 against this
build.

---

## 7. If the console is genuinely silent

Only after section 2 returns `SILENT, PORT ALIVE` with a confirmed-correct image
from section 3:

- `sdkconfig.defaults` currently sets `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`
  together with `CONFIG_ESP_CONSOLE_SECONDARY_NONE=y`. There is no fallback
  console, so a console fault and a boot fault are indistinguishable. Consider
  dropping `SECONDARY_NONE` for bring-up builds.
- Add a `coredump` partition and `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y`. A
  crash before console init still leaves a backtrace readable over the same USB
  connection with `espcoredump.py`.
- The ESP32-S3 built-in JTAG is on the same cable. `openocd -f
  board/esp32s3-builtin.cfg`, then `halt` and `reg pc`, tells you whether the
  CPU is in ROM or in your image without any console at all. On Windows this
  needs the WinUSB driver bound to the JTAG interface.

---

## Status at time of writing

G1 NOT PASS. G2 PENDING.

Established: CPU boots, partition layout correct, USB Serial/JTAG console
functional, TCA9554 responds and outputs go safe, controller FSM starts.

Not established: anything about `5438e84`, `5634196` or `01f978ff`, none of
which has been confirmed running on the board.
