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
  |- RS485 -> LAUMAS TLB485 -> load cell
  `- Web HMI cục bộ
```

Wi-Fi không nằm trong vòng điều khiển. Controller, cân và I/O đều cục bộ tại SP01.

## Chạy simulation trên Linux amd64

```bash
git switch fw-sp01-v0.1
git pull --ff-only

cmake -S firmware/host -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
./build/host/sp01_host
```

Xem HMI preview và bộ tham số hiện tại:

```bash
./firmware/host/serve-hmi.sh 8080
```

Mở:

```text
http://<ip-linux-node>:8080/
```

HMI trên Linux chỉ là **mock/read-only để review UI**; không có GPIO, Modbus hay quyền điều khiển actuator.

## Nạp firmware vào ESP32-S3

Dùng ESP-IDF v5.5.5:

```bash
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <PORT> flash monitor
```

Trong `menuconfig -> SP01 Filling Controller` cấu hình Wi-Fi SSID/password và service token. Lần flash đầu giữ `TLB calibration writes = disabled`.

Linux thường thấy port dạng:

```text
/dev/ttyACM0
/dev/ttyUSB0
```

Ví dụ:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Windows ví dụ:

```powershell
idf.py -p COM6 flash monitor
```

Sau khi ESP vào Wi-Fi, mở:

```text
http://<ip-esp32>/
```

Chi tiết đầy đủ về HMI, parameter, flash và hardware gate: [`firmware/README.md`](firmware/README.md).

## Chế độ vận hành

### MANUAL

Máy đứng yên. `DI04 process.initiative` dùng làm công tắc ON/OFF nạp bao.

```text
OFF -> nghỉ
ON  -> giữ bao -> cân -> nạp thô -> nạp tinh -> đủ cân -> dừng
```

Ở MANUAL:

- không cần tín hiệu máy quay;
- không cần conveyor ready;
- bỏ qua fill-position và 2 cảm biến discharge;
- tuyệt đối không kích `bag.push`;
- đủ cân thì dừng ở COMPLETE, không tự chạy chu kỳ kế tiếp;
- gạt OFF thì dừng nạp và trở về trạng thái chờ.

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

Hai cảm biến discharge dùng để loại bỏ sai số do tốc độ quay thay đổi. Firmware đo thời gian A->B rồi chuẩn hoá countdown. `discharge_lead` chỉ hiệu chỉnh vị trí, không dùng fixed delay theo một tốc độ danh định.

## Lưu ý hiệu chuẩn

DI7 và DI8 hiện là `discharge_ref_a` và `discharge_ref_b`, **không phải service/calibration switch**.

Calibration từ Web HMI chỉ được phép khi đồng thời thỏa điều kiện service: máy dừng, fill switch OFF, controller ở trạng thái an toàn, output OFF, cân ổn định và dữ liệu cân còn mới, calibration writes đã enable, service token hợp lệ.

## Nhánh

| Branch | Mục đích |
|---|---|
| `main` | spec và integrated baseline hiện hành |
| `fw-sp01-v0.1` | nhánh tích hợp firmware SP01 |
| `py-sim` | digital twin Python / replay / đối chiếu |
| `debate` | lịch sử thảo luận và red-team |

## Tài liệu cần đọc

- [`spec/SP01.md`](spec/SP01.md) — trình tự, mode và I/O chuẩn
- [`docs/HW.md`](docs/HW.md) — kiến trúc và đấu nối prototype
- [`docs/BOM.md`](docs/BOM.md) — danh sách mua cho 1 node
- [`docs/CALIBRATION.md`](docs/CALIBRATION.md) — hiệu chuẩn cân
- [`docs/FW.md`](docs/FW.md) — gate và milestone firmware
- [`firmware/README.md`](firmware/README.md) — simulation, HMI, parameter và flash
- [`hardware/README.md`](hardware/README.md) — link hãng, manual, ảnh

## Trình tự hardware gate

```text
Linux amd64 simulation + review HMI
-> ESP boot / tất cả DO phải OFF
-> dummy DI/DO 24 V
-> TLB485 + load cell
-> hiệu chuẩn 0 / 20 / 50 kg
-> dry cycle MANUAL/AUTO
-> test mất Wi-Fi / reboot / fault
-> shadow SP01
-> kết nối máy có kiểm soát
```
