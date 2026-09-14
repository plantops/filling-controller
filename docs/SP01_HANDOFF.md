# SP01 — handoff

Ngày: 2026-09-15
Repo: `plantops/filling-controller`
Nhánh đang làm: `feat/sp01-from-309`
Head: `95e42907`

---

## Việc tiếp theo là gì

**Không viết code.** Chờ chủ máy điền `docs/SP01_ROBOT_TEMPLATE.md`.

Lý do dừng: bản mô tả phần cứng và logic hiện có chắp vá từ nhiều nguồn, mâu thuẫn
nhau. Trong phiên trước đã code dựa trên suy đoán nhiều lần và sai nhiều lần. Xem
phần "Những chỗ đã đoán sai" bên dưới.

Khi bản mô tả xong, đối chiếu từng dòng với code hiện có: chỗ nào khớp thì giữ, chỗ
nào lệch thì **sửa theo bản mô tả, không sửa theo code**.

---

## Trạng thái kỹ thuật

### Nhánh và CI

`feat/sp01-from-309` tách từ `097c58cf` (FW #309, bản cuối đã kiểm chứng vật lý).
CI xanh cả hai job ở lần chạy gần nhất có thay đổi firmware.

Đã nạp lên board và chạy: đổi chế độ sang FULL_SW hoạt động, output vật lý khoá
đúng, FSM đi hết chuỗi tới `COARSE_FILL`.

**Chưa kiểm chứng vật lý:** bàn phím PIN 6 số, nút Xoá lỗi, nút RESET, timeline đọc
từ `/api/trace`, chế độ SIMU, và việc DO vật lý đứng yên khi ở SIMU. Đây là danh
sách cần thử khi quay lại board.

### Thành phần đã có, đều có test host

| Component | Nội dung | Test |
|---|---|---|
| `position` | giải mã 5 cảm biến vị trí từ 2 chân, theo thời gian sau xung index | 8 |
| `controller` | FSM + `controller_explain` tính mọi giá trị dẫn xuất trong C++ | 15 |
| `timekeep` | giờ lấy từ browser, ca 0/8/16, bộ đếm không reset khi chưa biết giờ | 21 |
| `trace` | ring buffer 1024 sự kiện, ghi cạnh tại control tick | 11 |
| `testsource` | ba nguồn REAL_HW / FULL_SW / SIMU | 20+ |
| `plausibility` | 7 luật phát hiện tín hiệu vô lý | 15 |

`plausibility` **chưa được nối vào `app_main`** — cố ý, vì nó đụng vào permissive
và đang chờ chốt chuyện DI2.

### Nguyên tắc kiến trúc đã thiết lập

Trình duyệt chỉ vẽ, không suy luận. Mọi giá trị dẫn xuất — permissive, danh sách lý
do chặn, trạng thái kế tiếp, mặt nạ output mong muốn — tính trong `controller_explain.cpp`
cạnh controller. Bản HMI của team trước giữ một bản sao luật permissive và bảng
output bằng JavaScript, và nó lệch khỏi firmware mà không ai biết.

FULL_SW và SIMU **không bao giờ** điều khiển được output vật lý. Không có cấu hình
nào mở ra. Cưỡng bức trong `app_main`:

```cpp
if (!g_source.output_authority_possible()) {
    physical_outputs = sp01::safe_output_image();
}
```

FULL_SW chạy controller bằng đồng hồ riêng chỉ nhích khi bấm. Timeout giữ nguyên
giá trị thật — không nới để dễ test.

### Ba nút trên DEV, ba vai trò khác nhau

| Nút | Có thể bị từ chối |
|---|---|
| Xoá lỗi | Có — cần tắt initiative, và bao loại phải rời vòi |
| RESET | Không bao giờ |
| Đổi nguồn | Có — PIN supervisor, và phải ở WAIT_PERMISSIVE / FAULT / COMPLETE |

PIN operator `1111`, supervisor `111111`, đang nằm trong Kconfig chứ không phải NVS.
Ai có file binary đọc được bằng `strings`. Spec yêu cầu NVS — đây là khoảng cách
còn lại. Sai 5 lần khoá 30 giây, áp cho cả hai endpoint.

---

## Những chỗ đã đoán sai trong phiên trước

Ghi lại vì chúng cùng một dạng: đặt ràng buộc hoặc giả định mà không hỏi.

1. **DI2 nằm trong permissive** — chủ máy nói nó chỉ ảnh hưởng việc nhả bao, không
   ảnh hưởng việc nạp. Code hiện tại vẫn sai, chưa sửa.
2. **Timeout theo giây thay vì theo vòng** — luật thật là "chưa đủ cân thì chờ vòng
   sau, quá 2 vòng thì dừng motor báo lỗi". Coarse timeout 12 s nổ trước khi hết
   vòng đầu (14.4 s), nên luật 2 vòng không bao giờ chạy tới.
3. **`reject_wait_timeout_us` để 0** — tức tắt hẳn, trong khi `wait_discharge` có
   20 s. Bất đối xứng do sơ ý. Đã sửa.
4. **Đổi nguồn chỉ cho phép từ WAIT_PERMISSIVE** — lỗi phát sinh trong FULL_SW làm
   việc rời FULL_SW bất khả. Đã sửa, nới thêm FAULT và COMPLETE.
5. **`clear_fault` quên bao đã loại** — reset disposition trong khi bao rách vẫn
   nằm trên vòi. Đã sửa: từ chối xoá lỗi khi còn bao loại.
6. **Xoá nhầm `control_task`** khi cắt khối HMI cũ — phải dựng lại từ bản gốc.
7. **`SP01_VIRTUAL_IO` là config chết** — help text hứa một thuộc tính an toàn
   không tồn tại. Đã xoá.
8. **Bản mẫu mô tả bị ép vào khuôn 8 vào 8 ra** — khiến nút ON/OFF trên vòi và van
   kẹp bao bị coi như ngoại lệ, trong khi chúng là bộ phận bình thường của máy. Đã
   sửa: liệt kê tín hiệu trước, gán chân sau.

---

## Những chỗ còn chưa biết

| Chưa biết | Chặn việc gì |
|---|---|
| DI6 là cảm biến đọc về, hay tín hiệu giữ xi lanh kẹp trong mạch máy | luật vô lý cho DI6, và toàn bộ chuỗi nhận bao |
| Nút ON/OFF trên vòi có vào bộ não không, vào chân nào | chế độ MANUAL nạp bao tại chỗ |
| Van kẹp bao do ai điều khiển | có cần thêm kênh ra không |
| Tới điểm nhả mà băng tải chưa sẵn sàng thì làm gì | luật nhả bao |
| Bao ON liên tục quá bao nhiêu vòng là kẹt | luật vô lý cho DI6 |
| Van tắt thì đóng hay mở | định nghĩa trạng thái an toàn, mà toàn bộ xử lý lỗi dựa lên |

Ô cuối quan trọng nhất. Code đang coi "tất cả output tắt" là an toàn, chưa ai xác nhận.

---

## Hai hạn chế của môi trường làm việc

**Không đọc được log CI trực tiếp.** GitHub phục vụ log Actions từ Azure blob,
ngoài allowlist mạng. Đã dựng cơ chế: khi build lỗi, workflow đăng phần `error:`
thành commit comment, đọc được qua `api.github.com`. Cơ chế này hoạt động trên
nhánh này. Nếu nó im lặng, phải nhờ người dán log — đừng đoán, đã mất nhiều lượt
vì đoán.

**Không kiểm tra vật lý được.** Mọi khẳng định về hành vi trên board phải do người
vận hành xác nhận. Đừng tuyên bố gate vật lý đã qua dựa trên CI hay mô phỏng.

---

## Cách làm việc mà chủ máy muốn

Nhanh, gọn, không overengineering. Có vấn đề cụ thể thì sửa thẳng, chạy CI, báo
commit và kết quả. Không dừng lại ở việc viết thêm tài liệu thiết kế.

Trả lời bằng tiếng Việt. Thuật ngữ kỹ thuật và tên định danh giữ tiếng Anh.

Ngoại lệ hiện tại: đang ở giai đoạn cố ý dừng code để làm rõ mô tả. Đừng tự ý viết
code tiếp khi các ô `?` ở trên còn chặn đường.

---

## Việc dở dang

- Màn hình hiệu chuẩn (ZERO → quả chuẩn 20/40/50 → DONE) đã dựng xong giao diện,
  **chưa commit**. Chủ máy đã duyệt trình tự. Cần chặn khi máy chưa an toàn và khi
  đang chạy dummy weight.
- Manual DO test mới có endpoint, chưa có giao diện.
- Machine ID và Spout ID mặc định `MAY ?` / `VOI ?`, phải chuyển sang NVS.
- `dev.html` chưa hiển thị kết quả của `plausibility` vì component chưa nối.

---

## Một chuyện về bảo mật

Trong phiên trước, một GitHub PAT được dán vào cuộc trò chuyện và đã dùng để thao
tác repo. Token đó cần thu hồi. Phiên mới nên dùng `gh auth login` thay vì dán
token vào chat.
