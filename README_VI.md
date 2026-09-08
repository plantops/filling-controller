# Filling Controller — Hướng dẫn nhanh

Bộ điều khiển mở cho máy đóng bao xi măng rotary 8 vòi.

## Trạng thái hiện tại

Đang làm **prototype SP01** trên bàn thử. Chưa điều khiển máy thật.

```text
Laptop
  |
AP/router Wi-Fi riêng
  )))
  )))  chỉ giám sát

ESP32-S3 SP01 (trên phần quay)
  |- 8 DI / 8 DO cục bộ
  |- RS485 -> LAUMAS TLB485 -> load cell
  |- FreeRTOS controller
  `- Web HMI
```

Wi-Fi không nằm trong vòng điều khiển. Mất Wi-Fi, AP hoặc laptop không được làm thay đổi chu trình đóng bao.

## Phần cứng v0.1

- ESP32-S3 8DI/8DO, RS485, Wi-Fi.
- LAUMAS TLB485.
- 1 AP/router Wi-Fi 2.4 GHz đặt cố định.
- Nguồn 24 VDC: giả định có sẵn, phải đo kiểm trước khi nối máy.
- Test đầu tiên dùng switch 24 V cho DI và tải giả cho DO.

Chi tiết: [`docs/HW.md`](docs/HW.md), [`docs/BOM.md`](docs/BOM.md).

## I/O SP01

```text
DI1 hopper.feeder_running
DI2 downstream.conveyor_ready
DI3 machine.motor_running
DI4 process.initiative
DI5 cycle.fill_position
DI6 bag.present
DI7 position.push
DI8 spare

DO1 scanner.down
DO2 bag_detect_air
DO3 bag.push
DO4 dosing.valve_a
DO5 dosing.valve_b
DO6 dosing.valve_c
DO7 filling.motor
DO8 spout.aeration
```

Dosing:

```text
OFF     A=0 B=0 C=0
FINE    A=1 B=0 C=1
COARSE  A=1 B=1 C=1
```

## Hiệu chuẩn cân

```text
1. Saddle rỗng -> ZERO
2. 20 kg -> CHECK
3. Quả chuẩn 50 kg -> SET SPAN
4. Bỏ tải -> VERIFY ZERO
5. 20 kg -> VERIFY
6. 50 kg -> VERIFY
7. SAVE
```

Chỉ hiệu chuẩn khi máy dừng, output bị khóa và tín hiệu cân ổn định.

Chi tiết: [`docs/CALIBRATION.md`](docs/CALIBRATION.md).

## Phần mềm

- `main`: tài liệu/spec hiện hành.
- `py-sim`: mô phỏng Python và digital twin.
- `fw-sp01-v0.1`: firmware ESP32-S3 bằng ESP-IDF/C++/FreeRTOS.
- `debate`: lịch sử thảo luận, red-team và phương án đã loại.

Trình tự làm việc:

```text
dummy DI/DO
-> TLB + calibration
-> dry cycle
-> Wi-Fi stress
-> shadow trên SP01
-> review
-> mới thử output thật
```

Firmware plan: [`docs/FW.md`](docs/FW.md).
