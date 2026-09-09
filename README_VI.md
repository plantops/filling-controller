# Filling Controller — Tiếng Việt

Bộ điều khiển mở cho máy đóng bao xi măng rotary 8 vòi.

**Mục tiêu hiện tại: SP01 hardware-ready để chạy bench.**

```text
PHẦN CỐ ĐỊNH
Linux/laptop -> AP/router Wi-Fi riêng
                      ))) chỉ giám sát

PHẦN QUAY SP01
ESP32-S3 / ESP-IDF / C++ / FreeRTOS
  |- 8 DI / 8 DO
  |- isolated RS485 -> LAUMAS TLB485 -> load cell
  `- Web HMI cục bộ
```

Wi-Fi không nằm trong vòng điều khiển. Controller, cân và I/O đều cục bộ tại SP01.

## Trạng thái hiện tại

[`VERSION`](VERSION) = **`0.1.0-rc1`**. Tag `v0.1.0-rc1` vẫn là baseline RC đã đóng băng; `main` có thể chứa các cập nhật thiết kế/tài liệu sau RC.

Phần mềm đã **READY FOR BENCH**. Rủi ro kỹ thuật cần đo tập trung nhất hiện nay là lấy tín hiệu cân số từ TLB485 lên ESP ổn định, nhanh và có xử lý stale/fault rõ ràng.

Xem [`progress.md`](progress.md).

## Thiết kế tín hiệu cân — đã chốt

```text
load cell bridge
   -> LAUMAS TLB485
   -> cổng isolated RS485 trên board ESP32
   -> WeightSnapshot số
   -> controller
```

Nguyên tắc:

- ESP32 production không đọc trực tiếp tín hiệu mV/V của load cell;
- **không dùng DI để truyền giá trị cân liên tục**;
- cả 8 DI giữ cho tín hiệu máy;
- task TLB đọc Modbus độc lập, controller không block chờ RS485;
- bring-up ban đầu: 9600 bit/s, address 1, poll 50 ms;
- sau khi G4 đo tốt: mục tiêu 115200 bit/s, poll 20 ms, khoảng 50 mẫu/s;
- poll 10 ms chỉ thử khi latency/error/timing thực tế cho phép.

Chi tiết canonical: [`docs/WEIGHING.md`](docs/WEIGHING.md).

## Recipe target

Giữ cách vận hành đã proven: các bộ recipe khác nhau chủ yếu ở target, các tham số filling đã tune giữ nguyên nếu không có lý do thay đổi.

```text
50.0 kg
50.1 kg
50.2 kg
50.3 kg
...
```

Kiểm bao bằng cân ngoài rồi đổi target rất nhanh để adapt thực tế. Đây là **operating target**, không phải calibration. v0.1 không cần PID hay AI tự học target.

## Chạy simulation trên Linux amd64

```bash
git switch main
git pull --ff-only

cmake -S firmware/host -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
./build/host/sp01_host

./firmware/host/serve-hmi.sh 8080
```

Mở `http://<ip-linux-node>:8080/` để xem HMI preview, parameter và UX recipe. HMI Linux là mock/read-only, không có GPIO/Modbus/actuator authority.

## Nạp firmware vào ESP32-S3

Dùng ESP-IDF v5.5.5:

```bash
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <PORT> flash monitor
```

Lần flash đầu giữ `TLB calibration writes = disabled`. Cấu hình baud/address/poll của ESP phải khớp với TLB.

Linux thường thấy `/dev/ttyACM0` hoặc `/dev/ttyUSB0`; Windows dùng COM tương ứng.

Chi tiết: [`firmware/README.md`](firmware/README.md).

## Chế độ vận hành

### MANUAL

Máy đứng yên. `DI04 process.initiative` dùng làm công tắc ON/OFF nạp bao.

```text
OFF -> nghỉ
ON  -> giữ bao -> cân -> nạp thô -> nạp tinh -> đủ cân -> dừng
```

Ở MANUAL không cần conveyor ready/fill position/discharge refs và tuyệt đối không kích `bag.push`.

### AUTO

Máy quay. Vòi tự chạy liên tục:

```text
permissive
-> vị trí nạp
-> nhận bao
-> cân/nạp
-> đủ cân
-> ref A
-> ref B
-> countdown theo tốc độ thực
-> đẩy bao
-> chu kỳ tiếp theo
```

Ở chu kỳ danh định 14.4 s/rev, điểm đổ là cố định sau khi tune geometry + actuator lead. Khi tốc độ đổi, thời gian delay scale theo tốc độ quay. Hai sensor A/B hiện giữ lại để đo tốc độ ngay trong revolution hiện tại; về nguyên lý một sensor + đo chu kỳ quay cũng có thể đủ nếu field evidence xác nhận.

## Hiệu chuẩn

Calibration và target recipe là hai việc riêng. Không sửa zero/span để bù target production.

DI7 và DI8 là `discharge_ref_a` và `discharge_ref_b`, không phải service/calibration switch.

## Nhánh

| Branch | Mục đích |
|---|---|
| `main` | integrated design/docs/firmware baseline hiện hành |
| `fw-sp01-v0.1` | nhánh phát triển firmware SP01; sync khi yêu cầu |
| `py-sim` | digital twin Python / replay / đối chiếu |
| `debate` | lịch sử thảo luận và red-team |

## Tài liệu cần đọc

- [`progress.md`](progress.md) — trạng thái, gate và việc tiếp theo
- [`spec/SP01.md`](spec/SP01.md) — trình tự, mode và I/O chuẩn
- [`docs/WEIGHING.md`](docs/WEIGHING.md) — load cell/TLB485/RS485 và recipe target
- [`docs/HW.md`](docs/HW.md) — kiến trúc/đấu nối prototype
- [`docs/BOM.md`](docs/BOM.md) — BOM 1 node
- [`docs/CALIBRATION.md`](docs/CALIBRATION.md) — hiệu chuẩn
- [`docs/FW.md`](docs/FW.md) — kiến trúc/gate firmware
- [`firmware/README.md`](firmware/README.md) — simulation, HMI, parameter và flash
- [`hardware/README.md`](hardware/README.md) — link hãng, manual, ảnh

## Trình tự hardware gate

```text
Linux amd64 simulation + review HMI
-> ESP boot / tất cả DO phải OFF
-> dummy DI/DO 24 V
-> TLB485 digital weight qua isolated RS485
-> hiệu chuẩn 0 / 20 / 50 kg
-> dry cycle MANUAL/AUTO
-> test mất Wi-Fi / reboot / stale / comm fault
-> shadow SP01
-> kết nối máy có kiểm soát
```
