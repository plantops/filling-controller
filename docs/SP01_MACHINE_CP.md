# SP01 — mô tả máy (v3)

Nguồn: Claudius Peters, *Rotary Packer and Star Feeder*, 02-1/158.215-352-A-00-en,
TCEC for Chinfon Haiphong Cement II, 2007/2008 — cộng với xác nhận của Thanh.

Ký hiệu: `[M-nnn]` = trang nnn của manual · `[T]` = Thanh xác nhận ·
`[I]` = suy luận · `?` = chưa biết.

---

## 1. Máy

| Mục | Trả lời |
|---|---|
| Máy | Rotary Packer **R8 ZML**, 8 vòi `[M-32]` |
| Số lượng | 4 packer, **cấu hình giống hệt nhau** `[T]` |
| SP01 là gì | một filling module — cân đóng bao tự động độc lập `[M-157]` |
| Bộ điều khiển hiện tại | PACTRON DIALOG 165B, một bộ mỗi vòi — **SP01 sẽ thay hẳn** `[T]` |
| Nhiệm vụ (1 câu) | Trong một vòng quay: kẹp bao người treo vào vòi, nạp xi măng tới 50 kg, nhả bao xuống băng tải |
| Một vòng quay | **15 s** `[T]` — tức mỗi vòi ~240 bao/h, cả máy ~1900 bao/h `[I]` |
| Ai đứng cạnh máy | công nhân treo bao tại vòi; công tắc giật dây quanh máy `[M-32]` |

---

## 2. Tín hiệu vào

| Tên | Thiết bị | Kiểu | ON nghĩa là | Nguồn |
|---|---|---|---|---|
| `bag.present` | **PE-converter** — khí thổi qua lỗ trong cao su giữ bao; có bao thì áp suất tăng | mức | bao đã kẹp đúng trên vòi | `[M-157, M-169]` |
| `prox.start` | tiệm cận Pepperl+Fuchs NBB8085501, đi qua switching vane cố định | xung mỗi vòng | vòi tới vị trí bắt đầu nạp | `[M-157, M-171]` |
| `prox.discharge` | tiệm cận thứ hai, bị kích bởi control cylinder cố định | xung mỗi vòng | tới vị trí nhả **và** hạ nguồn cho phép | `[M-157, M-171]` |
| `air.active` | công tắc áp khí (I01 của PACTRON) | mức | đủ khí 5,5–6 bar | `[M-303, M-168]` |
| Nút tại vòi | Start · Stop · Discharge · Blow out | xung | thao tác tay | `[M-171]` |
| Đèn tại vòi | Overload — turbine quá tải | ra | — | `[M-171]` |

**Interlock cơ khí:** điều kiện "băng tải sẵn sàng" không đi vào phần mềm. Nó làm xi
lanh cố định duỗi ra; xi lanh đó mới kích `prox.discharge` `[M-157]`. SP01 kế thừa
interlock này miễn là đọc đúng `prox.discharge`.

---

## 3. Cân

| Mục | Trả lời | Nguồn |
|---|---|---|
| Loadcell | HBM Z6 — **giữ nguyên**, analog | `[T]` |
| Chuỗi đo mới | HBM Z6 → **LAUMAS TLB** → RS485 → ESP32 | `[T]` |
| Khối lượng đích | **50 kg và 40 kg**, chọn theo công thức (recipe) trong HMI mới — không dùng 6 bit sort của bộ cũ | `[T]` |
| Họ công thức | 50.1–50.7 và 40.1–40.6 | `[T]` |
| Dung sai | **±0,20 kg** | `[T]` |
| Chuyển thô → tinh | **42,00 kg** cho bao 50 kg; bao 40 kg: `?` | `[T]` |
| Đóng cửa (điểm cắt) | 50 − (lượng rơi tiếp) | `[M-250]` |
| Lượng rơi tiếp (after-flow) | `?` — phải đo, xem ghi chú dưới | |
| "Đứng yên" (no motion) | 0,4 s ở cài đặt xưởng | `[M-320]` |
| Quy 0 (tare) | bộ cũ tare sau mỗi N bao, không phải mỗi bao | `[M-242, M-255]` |
| Chặn đánh giá sau khi nhả | 0,50 s ở cài đặt xưởng | `[M-320]` |

**After-flow là gì:** lượng xi măng vẫn còn rơi vào bao *sau khi* cửa chặn đã đóng —
phần đang bay trong ống, chưa chạm cân. Máy phải đóng cửa **sớm** đúng bằng lượng đó.
Bộ PACTRON nhận một giá trị khởi đầu rồi tự hiệu chỉnh sau mỗi N bao `[M-250, M-255]`.

Cách đo: đặt after-flow = 0 (cửa đóng đúng tại 50,00), chạy 10 bao, cân lại ngoài
máy. Trung bình chênh lệch so với 50 kg chính là lượng rơi tiếp. `[I]` Với cột nạp
ngắn của R8 ZML tôi đoán 0,3–1,0 kg, nhưng đo mới biết.

---

## 4. Tín hiệu ra

| Tên | Điều khiển vật gì | Kiểu | Mất điện | Nguồn |
|---|---|---|---|---|
| `feed.main` | xi lanh cửa chặn — vị trí mở thô | giữ | **đóng** | `[M-170]`, `[T]` |
| `feed.dribble` | **cùng xi lanh** — vị trí mở tinh | giữ | **đóng** | `[M-170]`, `[T]` |
| `bag.holder` | xi lanh kẹp bao vào vòi | giữ | **nhả** `[I]` | `[M-169]` |
| `bag.discharge` | xi lanh nhả bao, lật khung nghiêng | xung | về vị trí lò xo | `[M-157]` |
| `blow.out` | thổi sạch ống nạp, xung định thời | xung | đóng | `[M-157]` |
| `aeration` | sục khí labyrinth + thùng nạp | giữ | đóng | `[M-157]` |
| `turbine` | contactor động cơ turbine nạp — **SP01 điều khiển**, đóng **sau khi** cửa nạp đã mở `[T]` | giữ | dừng | `[M-173]` |
| `alarm.lamp` | đèn đỏ tại vòi | giữ | tắt | `[M-171]` |

Toàn bộ van điện từ là loại một cuộn, lò xo hồi, **mất điện là đóng** `[T]`.
Cửa chặn là **một xi lanh ba vị trí** — đóng / thô / tinh, không phải hai van `[M-170]`.

---

## 5. Chu kỳ

```
1. prox.start đi qua switching vane
   → xi lanh kẹp bao duỗi, thổi khí qua lỗ trong cao su giữ bao

2. có bao: áp suất tăng → PE-converter báo có bao
   không có bao: khí thoát qua lỗ đối diện → KHÔNG nạp,
   xi lanh nhả bao lật khung nghiêng để gạt bao treo sai khỏi vòi

3. turbine chạy, cửa chặn mở vị trí thô, sục khí labyrinth + thùng

4. thổi ống nạp bằng một xung định thời

5. cân đạt 42,00 kg → cửa chuyển sang vị trí tinh

6. cân đạt (50,00 − rơi tiếp) → cửa đóng

7. chờ ổn định → đánh giá: trong ±0,20 kg là đạt

8. control cylinder cố định duỗi khi băng tải và vận chuyển bao đang chạy
   → kích prox.discharge → nhả bao nếu bao đã đầy

9. khối lượng tụt dưới ngưỡng nhả → kết thúc chu kỳ, sẵn sàng bao mới
```

Ngân sách thời gian: một vòng 15 s, trong đó nạp + ổn định + nhả phải xong `[I]`.

---

## 6. Bất thường và điều cấm

| Tình huống | Nhận ra bằng | Nguồn |
|---|---|---|
| bao rách | tốc độ nạp tụt dưới ngưỡng, giữ quá thời gian đặt (xưởng 1,5 s) | `[M-279, M-317]` |
| nạp quá lâu | vượt thời gian nạp cho phép | `[M-277]` |
| ngoài dung sai | chuyển sang "stop full", **chặn nhả bao**, phải xác nhận | `[M-277]` |
| mất khí nén | công tắc áp khí OFF quá thời gian đặt | `[M-293]` |
| cân thiếu tải | gross âm quá 1 s | `[M-295]` |
| tín hiệu loadcell hỏng | ngoài dải đo | `[M-298]` |
| turbine quá tải | đèn đỏ tại vòi | `[M-171, M-173]` |
| bao treo sai / rơi khỏi vòi | không có áp suất → không nạp, lật khung nghiêng | `[M-157]` |

**Không bao giờ:**

```
1. Không bao giờ mở cửa nạp khi chưa có báo có bao.
2. Không bao giờ nhả bao khi prox.discharge chưa tích cực.
3. Không bao giờ nhả bao khi cân ngoài ±0,20 kg mà chưa có người xác nhận.
4. Không bao giờ tự nạp lại sau khi có điện lại mà chưa có người xác nhận.
```

| Dừng khẩn | Trả lời |
|---|---|
| Cắt gì | **động cơ turbine nạp + toàn bộ van điện từ** `[T]` |
| Có đi qua bộ não không | không — cắt cứng `[T]` `[I]` |
| Khi cắt thì máy ở đâu | van mất điện → **cửa nạp đóng** `[T]` |
| Bộ não treo (không ai bấm dừng khẩn) | **van vẫn giữ nguyên trạng thái đang có** — nếu treo lúc cửa đang mở thì cửa vẫn mở `[I]`. Đây là lỗ hổng duy nhất còn lại; cần watchdog phần cứng cắt nguồn van khi bộ não ngừng gõ nhịp. |

---

## 7. Chân — SP01 thay hẳn PACTRON

Giao diện 24 VDC 25 chân của PACTRON `[M-303]`, cấu hình thực tế máy này `[M-319]`,
là hợp đồng tín hiệu SP01 kế thừa **trừ phần chọn loại bao**: sáu bit sort bỏ hẳn,
công thức chọn trong HMI của SP01 `[T]`.

**Vào — tối thiểu 6:** `bag.present`, `prox.start`, `prox.discharge`, `air.active`,
nút Start, nút Stop.
**Vào — nên có thêm 2:** nút Discharge, nút Blow out.

**Ra — tối thiểu 7:** `feed.main`, `feed.dribble`, `bag.holder`, `bag.discharge`,
`blow.out`, `aeration`, `alarm.lamp`.
**Ra — thêm 1 nếu SP01 điều khiển turbine:** `turbine`.

Board hiện tại 8 DI / 8 DO: **vừa khít, không còn dự phòng**. Chưa tính ngõ ra
watchdog cắt nguồn van ở phần 6, và chưa tính tín hiệu báo lên PlantOps.

---

## 7b. Hai cảm biến, phần còn lại nội suy

Bản thiết kế 5 cảm biến là của máy HAVER. Máy CP có **2 cảm biến mỗi vòi** `[T]`:

| Cảm biến | Bản chất | Dùng được vào việc gì |
|---|---|---|
| `prox.start` | đi qua switching vane cố định, một xung mỗi vòng `[M-157]` | **mốc 0°** — chuẩn để nội suy toàn bộ góc |
| `prox.discharge` | bị kích bởi xi lanh cố định, xi lanh chỉ duỗi khi băng tải và vận chuyển bao đang chạy `[M-157]` | **vị trí nhả ĐÃ nhân với điều kiện cho phép** |

`prox.discharge` không phải một mốc góc thuần tuý: nó mang hai thông tin trộn làm một —
đúng vị trí, và hạ nguồn sẵn sàng. Không có xung thì không phân biệt được "chưa tới
vị trí" với "băng tải chưa chạy".

**Nội suy gỡ được chính chỗ đó:** nếu góc nội suy đã đi qua vị trí nhả mà xung không
về, nguyên nhân chỉ còn là hạ nguồn chưa sẵn sàng (hoặc cảm biến hỏng). Đó là một
chẩn đoán, không phải một suy đoán.

Quy tắc nội suy `[I]` — đề xuất, anh sửa:

```
chu kỳ T = khoảng cách giữa hai xung prox.start gần nhất
góc  = 360° × (t − t_index) / T

- Chưa đủ 2 xung index: KHÔNG có góc. Mọi quyết định theo góc bị cấm.
- T lệch quá ±10% so với vòng trước: coi như đang tăng/giảm tốc,
  góc chỉ dùng để hiển thị, không dùng để quyết định.
- Quá 1,5×T không thấy index: mất tín hiệu vị trí → đóng cửa nạp, báo lỗi.
```

**Phân vai `[I]`:** việc nhả bao vẫn để `prox.discharge` quyết — đó là interlock cơ
khí có sẵn, tin được hơn đồng hồ. Góc nội suy dùng cho giám sát, đặt thời hạn và
chẩn đoán. Như vậy khi phần mềm sai hoặc treo, máy không tự nhả bao sai chỗ.

`[T]` `bag.present` vẫn là PE-converter của CP — áp suất khí trong cao su giữ bao.

---

## 8. Chưa biết

| Chưa biết | Tìm ra bằng cách nào | Chặn việc gì |
|---|---|---|
| Lượng rơi tiếp thật (after-flow) | đo 10 bao với after-flow = 0 | độ chính xác |
| Tiếp điểm quá tải động cơ turbine có đấu về SP01 không | xem tủ vòi | phát hiện quá tải |
| Số lần khởi động động cơ cho phép mỗi giờ | hỏi nhà chế tạo động cơ / xem nhãn | xem ghi chú dưới |
| Điểm chuyển thô→tinh cho bao 40 kg | tune tại máy | phần 5 |
| Ngưỡng nhả (discharge threshold) hiện dùng | đọc từ PACTRON trước khi tháo | phần 5 bước 9 |
| Thời gian: nạp thô/tinh/ổn định chiếm bao nhiêu trong 15 s | đo tại máy | ngân sách chu kỳ |
| Watchdog cắt nguồn van: có sẵn gì chưa | xem tủ điện | an toàn |


---

## 9. Giá trị tạm đặt

Chưa đo được thì đặt tạm, nhưng **mỗi giá trị tạm phải tự khai báo là tạm** — trong
config và trên màn hình. Một con số tạm mà trông giống con số đo được là thứ nguy
hiểm nhất trong cả hệ thống.

| Tham số | Giá trị tạm | Cơ sở | Sai thì hỏng gì |
|---|---|---|---|
| `index_debounce_ms` | 20 | `[S]` | nhỏ quá: đếm trùng một xung thành hai vòng. lớn quá: bỏ xung |
| `index_pulse_min_ms` | 50 | `[S]` | dưới ngưỡng coi là nhiễu, không phải vòng mới |
| `index_pulse_max_ms` | 600 | `[S]` | trên ngưỡng: vane bẩn, cảm biến kẹt ON |
| `rev_period_nominal_s` | 15,0 | `[T]` | gốc cho mọi thời hạn |
| `rev_period_jitter_pct` | 10 | `[S]` | lệch hơn mức này thì góc chỉ để xem, không để quyết định |
| `index_lost_factor` | 1,5 × T | `[S]` | quá lâu không thấy index → mất vị trí, đóng cửa nạp |
| `discharge_angle_deg` | 350 ± 30 | `[S]` | chỉ dùng để phân biệt "băng tải chưa sẵn sàng" với "chưa tới vị trí" |
| `turbine_start_delay_ms` | 200 | `[S]` | mở cửa rồi mới chạy cánh; ngắn quá thì cánh quay vào cửa chưa mở hẳn |
| `after_flow_kg` | 0,50 | giá trị xưởng của CP `[M-316]` | sai thì mọi bao lệch đều một lượng cố định, tự học sẽ kéo về |
| `coarse_to_fine_kg` (bao 40) | 32,0 | `[I]` giữ dải tinh 8 kg như bao 50 | dải tinh sai thì độ lặp lại kém |

### Quy tắc hiển thị

```
Mỗi tham số mang một nhãn nguồn, hiện ngay cạnh giá trị:
  ĐO       — đo tại máy, có ngày đo
  NHÀ CHẾ TẠO — lấy từ manual CP, có số trang
  TẠM      — do phần mềm tự đặt, chưa ai kiểm chứng

HMI có một trang liệt kê mọi tham số nhãn TẠM.
Còn dòng nào nhãn TẠM thì trang đó hiện cảnh báo thường trực.

Không một quyết định an toàn nào được dựa trên tham số nhãn TẠM.
Tham số TẠM chỉ được phép ảnh hưởng tới: hiển thị, cảnh báo, chẩn đoán.
Việc nhả bao vẫn do prox.discharge quyết (xem phần 7b).
```

`[I]` Nhãn nguồn nên nằm trong chính file config, không phải một bảng riêng — để
không có đường nào đổi giá trị mà quên đổi nhãn.


---

## 10. Một điều cần hỏi lại nhà chế tạo

Mỗi bao một lần đóng contactor turbine. Ở 15 s một vòng, đó là **240 lần khởi động
mỗi giờ cho mỗi vòi** — khoảng 2 triệu lần một năm nếu chạy hai ca.

`[Inference]` Động cơ lồng sóc khởi động trực tiếp thường được ghi cho vài chục lần
khởi động mỗi giờ, không phải vài trăm. Nếu con số đó đúng với động cơ turbine ở đây
thì hoặc máy gốc không tắt turbine giữa các bao, hoặc contactor và động cơ đang được
dùng quá giới hạn từ trước tới nay.

Cách kiểm chứng rẻ nhất: xem nhãn động cơ và hỏi vận hành xem contactor turbine phải
thay bao lâu một lần. Nếu đúng là quá giới hạn, phương án là để turbine chạy liên tục
suốt ca thay vì đóng cắt theo bao — nhưng đó là thay đổi so với thiết kế gốc, cần
quyết định có chủ ý chứ không phải mặc định của phần mềm.
