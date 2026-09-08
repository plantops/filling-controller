# Filling Controller — Tiếng Việt

Bộ điều khiển mở cho máy đóng bao xi măng rotary 8 vòi.

**Mục tiêu hiện tại: prototype SP01 trên bàn thử.**

```text
PHẦN CỐ ĐỊNH
Laptop -> AP/router Wi-Fi riêng
                    ))) chỉ giám sát

PHẦN QUAY SP01
ESP32-S3 / ESP-IDF / C++ / FreeRTOS
  |- 8 DI / 8 DO
  |- RS485 -> LAUMAS TLB485 -> load cell
  `- Web HMI cục bộ
```

Wi-Fi không nằm trong vòng điều khiển. Controller, cân và I/O đều cục bộ tại SP01.

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
-> đo tốc độ bằng ref A/ref B
-> tính điểm đẩy theo tốc độ thực
-> đẩy bao
-> chu kỳ tiếp theo
```

Hai cảm biến discharge dùng để loại bỏ sai số do tốc độ quay thay đổi: firmware đo thời gian A->B rồi countdown bằng cùng đơn vị chuẩn hoá. `discharge_lead` chỉ hiệu chỉnh vị trí, không dùng fixed delay theo một tốc độ danh định.

## Nhánh

| Branch | Mục đích |
|---|---|
| `main` | spec và tài liệu phần cứng hiện hành |
| `fw-sp01-v0.1` | firmware ESP32-S3 |
| `py-sim` | digital twin Python / replay / đối chiếu |
| `debate` | lịch sử thảo luận và red-team |

## Tài liệu cần đọc

- [`spec/SP01.md`](spec/SP01.md) — trình tự, mode và I/O chuẩn
- [`docs/HW.md`](docs/HW.md) — kiến trúc và đấu nối prototype
- [`docs/BOM.md`](docs/BOM.md) — danh sách mua cho 1 node
- [`docs/CALIBRATION.md`](docs/CALIBRATION.md) — hiệu chuẩn cân
- [`docs/FW.md`](docs/FW.md) — kế hoạch firmware
- [`hardware/README.md`](hardware/README.md) — link hãng, manual, ảnh

## Trình tự làm

```text
Linux amd64 simulation
-> dummy DI/DO 24 V
-> TLB + hiệu chuẩn load cell
-> dry cycle MANUAL/AUTO
-> stress Wi-Fi
-> shadow SP01
-> review
-> kết nối máy có kiểm soát
```
