# Installation of ESP-IDF and Tools on Windows, Linux Server and macOS

This guide is written for colleagues who may be new to ESP32 development.

Project target:

```text
Board:      Waveshare ESP32-S3-POE-ETH-8DI-8DO
Chip:       ESP32-S3
ESP-IDF:    v5.5.5
Repository: plantops/filling-controller
```

> **Important:** this project is pinned to **ESP-IDF v5.5.5**. Do not silently build it with the newest ESP-IDF just because an installer offers a newer version.

Official Espressif references:

- ESP-IDF v5.5 Get Started: https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32/get-started/
- Windows setup for v5.5: https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32/get-started/windows-setup.html
- Linux/macOS setup for v5.5: https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32/get-started/linux-macos-setup.html
- ESP-IDF releases: https://github.com/espressif/esp-idf/releases

---

# 1. First decide: do you really need the full ESP-IDF?

There are two types of users.

## A. Technician / operator — only flash an approved GitHub binary

You do **not** need the full ESP-IDF toolchain.

You only need:

```text
Python 3
esptool
USB data cable
approved GitHub Actions artifact
```

Typical install:

```bash
python -m pip install --upgrade esptool
```

Then flash the approved artifact as described in [`ONBOARDING.md`](ONBOARDING.md).

This is the preferred path for routine maintenance and spare-controller replacement.

## B. Developer / engineer — build firmware from source

Install full **ESP-IDF v5.5.5** using the instructions below.

You need full ESP-IDF when you want to:

```text
change firmware source
run menuconfig
build new firmware
change compile-time configuration
run idf.py build / flash / monitor
investigate low-level ESP issues
```

---

# 2. Common rules for all operating systems

## 2.1 Use v5.5.5 exactly

After installation, this command must show v5.5.5:

```bash
idf.py --version
```

Expected result is equivalent to:

```text
ESP-IDF v5.5.5
```

If it shows v6.x or another v5.x release, stop and activate/install the correct environment before building this project.

## 2.2 Avoid spaces in important paths

Prefer simple paths.

Good:

```text
C:\esp\
C:\work\filling-controller
/home/user/esp/
/home/user/src/filling-controller
/Users/user/esp/
/Users/user/src/filling-controller
```

Avoid paths such as:

```text
C:\My Projects\ESP Project\
```

ESP-IDF v5.5 documentation warns that Windows tool/IDF paths should not contain spaces or parentheses and should stay reasonably short.

## 2.3 Target is ESP32-S3

For this project always use:

```bash
idf.py set-target esp32s3
```

Do not use `esp32`, `esp32c3`, etc.

## 2.4 One shell = one activated IDF environment

`idf.py` works only after the correct ESP-IDF environment has been activated in that terminal session.

When in doubt:

```bash
idf.py --version
```

before building.

---

# 3. Windows 10 / 11

## Recommended Windows path

For a colleague new to ESP-IDF, use the official Espressif installer/tool environment. For this project **select v5.5.5 explicitly**.

Do not accept a default latest v6.x installation for SP01 firmware.

## 3.1 Option A — ESP-IDF Installation Manager (EIM)

Espressif's current Installation Manager can manage multiple IDF versions. Install EIM from Windows Package Manager if available:

```powershell
winget install Espressif.EIM
```

For CLI-only EIM:

```powershell
winget install Espressif.EIM-CLI
```

Then install the project version explicitly:

```powershell
eim install -i v5.5.5
```

If using the GUI, choose a custom/versioned installation and select **v5.5.5**, not simply "latest".

After installation, open the IDF terminal/environment created for v5.5.5 and run:

```powershell
idf.py --version
```

## 3.2 Option B — ESP-IDF Tools Installer for release/v5.5

The v5.5 Windows guide also supports the traditional ESP-IDF Tools Installer. It installs the required Python, Git, cross compiler, CMake, Ninja and ESP-IDF environment.

During setup:

```text
select ESP-IDF v5.5.5
use a short installation path
avoid spaces / parentheses
allow the installer to create the ESP-IDF PowerShell / Command Prompt shortcut
```

After installation, use the dedicated **ESP-IDF PowerShell** or **ESP-IDF Command Prompt** shortcut instead of an arbitrary PowerShell window.

Verify:

```powershell
idf.py --version
python --version
git --version
```

## 3.3 Clone the filling-controller repository

Choose a simple folder, for example:

```powershell
mkdir C:\work
cd C:\work
```

Then clone the private repository using your approved GitHub authentication method:

```powershell
git clone https://github.com/plantops/filling-controller.git
cd filling-controller
```

If your organization requires SSH instead:

```powershell
git clone git@github.com:plantops/filling-controller.git
cd filling-controller
```

## 3.4 First Windows build

Inside an activated **v5.5.5** ESP-IDF terminal:

```powershell
cd C:\work\filling-controller\firmware\esp32-s3
idf.py --version
idf.py set-target esp32s3
idf.py build
```

The first build downloads/configures more dependencies and can take longer than later builds.

## 3.5 Find the COM port

Connect the board by USB-C data cable.

Open Windows Device Manager:

```text
Device Manager
-> Ports (COM & LPT)
```

Record the port, for example:

```text
COM6
```

You can also compare available ports before and after plugging in the board.

## 3.6 Flash and monitor

With machine wiring disconnected:

```powershell
idf.py -p COM6 flash monitor
```

To leave monitor:

```text
Ctrl + ]
```

## 3.7 Common Windows problems

### `idf.py` is not recognized

You are probably not inside the activated IDF terminal.

Open the ESP-IDF v5.5.5 PowerShell/Command Prompt again and run:

```powershell
idf.py --version
```

### Wrong IDF version

If:

```text
idf.py --version
ESP-IDF v6.x
```

then you activated the wrong installation. Switch to v5.5.5 before building.

### Board has no COM port

Check, in order:

```text
1. USB cable is a DATA cable, not charge-only
2. try another USB port
3. unplug/replug board
4. check Device Manager
5. only then try BOOT/RESET download-mode fallback
```

Do not start by modifying drivers or firmware when the cable is unknown.

### Long/space-containing path errors

Move the source tree to a short location such as:

```text
C:\work\filling-controller
```

---

# 4. Linux server — Ubuntu / Debian

This is the recommended setup for a headless development/build node.

The server may build firmware without any ESP board physically attached. USB access is only required when you also want the server to flash/monitor a board.

## 4.1 Install prerequisites

```bash
sudo apt update
sudo apt install -y \
  git wget flex bison gperf \
  python3 python3-pip python3-venv \
  cmake ninja-build ccache \
  libffi-dev libssl-dev \
  dfu-util libusb-1.0-0
```

Check:

```bash
python3 --version
git --version
cmake --version
ninja --version
```

ESP-IDF v5.5 requires Python 3.9 or newer.

## 4.2 Install ESP-IDF v5.5.5

Keep the framework outside the application repository:

```bash
mkdir -p ~/esp
cd ~/esp

git clone -b v5.5.5 --recursive \
  https://github.com/espressif/esp-idf.git \
  esp-idf-v5.5.5
```

Install only the ESP32-S3 tools needed by this project:

```bash
cd ~/esp/esp-idf-v5.5.5
./install.sh esp32s3
```

This installs the Espressif toolchain and Python environment under the user's Espressif tools area.

## 4.3 Activate ESP-IDF in the current shell

Every new shell used for ESP development must activate v5.5.5:

```bash
source ~/esp/esp-idf-v5.5.5/export.sh
```

Verify:

```bash
idf.py --version
```

Expected:

```text
ESP-IDF v5.5.5
```

### Convenient server alias

Instead of automatically loading the large IDF environment into every shell, add an explicit alias to `~/.bashrc`:

```bash
alias idf55='source $HOME/esp/esp-idf-v5.5.5/export.sh'
```

Reload shell config:

```bash
source ~/.bashrc
```

Then each development session begins with:

```bash
idf55
idf.py --version
```

## 4.4 Clone the project

Example:

```bash
mkdir -p ~/src
cd ~/src

git clone git@github.com:plantops/filling-controller.git
cd filling-controller
```

Or HTTPS if that is how your GitHub access is configured.

## 4.5 Build on Linux server

```bash
source ~/esp/esp-idf-v5.5.5/export.sh
cd ~/src/filling-controller/firmware/esp32-s3

idf.py --version
idf.py set-target esp32s3
idf.py build
```

## 4.6 Headless server: build only

If there is no ESP board attached, stop after:

```bash
idf.py build
```

The server is still useful for:

```text
source builds
CI reproduction
menuconfig preparation
binary generation
firmware inspection
```

GitHub Actions remains the canonical shared CI build for the team.

## 4.7 Linux server with USB board attached

Find the serial device:

```bash
ls /dev/ttyACM* 2>/dev/null
ls /dev/ttyUSB* 2>/dev/null
```

You can also watch kernel events while plugging in the board:

```bash
dmesg -w
```

Typical devices:

```text
/dev/ttyACM0
/dev/ttyUSB0
```

If access is denied:

```bash
sudo usermod -aG dialout $USER
```

Then log out and log in again.

Flash:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Do not run the complete ESP development environment as root just to get serial access.

---

# 5. macOS — Intel and Apple Silicon

## 5.1 Install Apple command-line tools

```bash
xcode-select --install
```

If already installed, macOS will tell you.

## 5.2 Install Homebrew prerequisites

If Homebrew is already installed:

```bash
brew update
brew install git python cmake ninja dfu-util ccache
```

Verify:

```bash
python3 --version
git --version
cmake --version
ninja --version
```

## 5.3 Install ESP-IDF v5.5.5

```bash
mkdir -p ~/esp
cd ~/esp

git clone -b v5.5.5 --recursive \
  https://github.com/espressif/esp-idf.git \
  esp-idf-v5.5.5

cd ~/esp/esp-idf-v5.5.5
./install.sh esp32s3
```

## 5.4 Activate the environment

```bash
source ~/esp/esp-idf-v5.5.5/export.sh
idf.py --version
```

Expected:

```text
ESP-IDF v5.5.5
```

For zsh, an optional explicit alias in `~/.zshrc` is useful:

```bash
alias idf55='source $HOME/esp/esp-idf-v5.5.5/export.sh'
```

Then:

```bash
source ~/.zshrc
idf55
```

## 5.5 Clone and build the project

```bash
mkdir -p ~/src
cd ~/src

git clone git@github.com:plantops/filling-controller.git
cd filling-controller/firmware/esp32-s3

idf.py set-target esp32s3
idf.py build
```

## 5.6 Find the USB serial port on macOS

Connect the board and run:

```bash
ls /dev/cu.*
```

ESP devices often appear as something similar to:

```text
/dev/cu.usbmodemXXXX
/dev/cu.usbserial-XXXX
```

For command-line flashing on macOS, prefer the `/dev/cu.*` device.

Flash:

```bash
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

## 5.7 Apple Silicon note

ESP-IDF v5.5.x generally has modern macOS tooling, but if an older downloaded tool reports:

```text
bad CPU type in executable
```

Espressif documents Rosetta 2 as a compatibility fallback:

```bash
/usr/sbin/softwareupdate --install-rosetta --agree-to-license
```

Do not install Rosetta pre-emptively if the native toolchain works normally.

---

# 6. Verify a complete developer installation

On any OS, the environment is considered ready only when all of these pass:

```text
idf.py --version      -> ESP-IDF v5.5.5
git --version         -> works
python / python3      -> works
idf.py set-target     -> esp32s3 accepted
idf.py build          -> completes without error
```

For this repository:

```bash
cd filling-controller/firmware/esp32-s3
idf.py set-target esp32s3
idf.py build
```

A successful build produces, under `build/`, items such as:

```text
bootloader/bootloader.bin
partition_table/partition-table.bin
application .bin
flash_args
flasher_args.json
```

---

# 7. First project configuration

Before a locally built first-board flash:

```bash
idf.py menuconfig
```

Navigate to:

```text
SP01 Filling Controller
```

For first G1 bring-up:

```text
TLB calibration writes: OFF
machine actuators: physically disconnected
Wi-Fi credentials: optional for first USB boot
service token: set only if service/calibration UI is intentionally used
```

No TLB is required for the current first-board G1 session. Missing TLB communication must remain a safe condition.

---

# 8. Cleaning after changing IDF version or target

If someone accidentally built with another IDF version, do not reuse the old build directory.

Activate v5.5.5 and run:

```bash
idf.py fullclean
idf.py set-target esp32s3
idf.py build
```

If the build directory is disposable, deleting `firmware/esp32-s3/build/` is also acceptable before a clean reconfigure.

---

# 9. Developer build versus GitHub CI build

Local developer build:

```text
useful for debugging / menuconfig / flashing from source
```

GitHub Actions build:

```text
canonical shared build evidence
repeatable ESP-IDF v5.5.5 environment
produces downloadable SP01 flash artifact
```

The project CI pins:

```text
ESP-IDF v5.5.5
target esp32s3
```

A local machine should reproduce that version before investigating a build difference.

---

# 10. Minimal commands — quick reference

## Windows developer

```powershell
# Open the ESP-IDF v5.5.5 PowerShell/Command Prompt first
idf.py --version
cd C:\work\filling-controller\firmware\esp32-s3
idf.py set-target esp32s3
idf.py build
idf.py -p COM6 flash monitor
```

## Ubuntu/Debian server developer

```bash
source ~/esp/esp-idf-v5.5.5/export.sh
cd ~/src/filling-controller/firmware/esp32-s3
idf.py --version
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor   # only when board is attached
```

## macOS developer

```bash
source ~/esp/esp-idf-v5.5.5/export.sh
cd ~/src/filling-controller/firmware/esp32-s3
idf.py --version
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

## Technician — artifact only

```bash
python -m pip install --upgrade esptool
python -m esptool --chip esp32s3 -p <PORT> write_flash @flash_args
```

See [`ONBOARDING.md`](ONBOARDING.md) before applying power or connecting any field I/O.
