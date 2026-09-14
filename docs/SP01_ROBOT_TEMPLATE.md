# SP01 — bản mô tả con robot

Trong tài liệu này, **bộ não** là thứ đọc giác quan và ra lệnh cho cơ bắp. Không
gọi tên con chip hay tên board ở bất cứ đâu ngoài phần 11. Đổi phần cứng thì chỉ
phần 11 phải sửa, chín phần còn lại vẫn đúng.

Điền xong bản này thì phần mềm chỉ còn là phiên dịch. Chưa điền xong thì không viết
code, vì mọi chỗ trống sẽ được lấp bằng suy đoán, và suy đoán sai không tự lộ ra.

Quy ước điền:

- Ô nào chắc chắn thì ghi thẳng.
- Ô nào chưa biết thì ghi `?` — **đừng đoán cho đầy**. Một ô `?` là an toàn;
  một ô sai là nguy hiểm.
- Ô nào "thường là vậy nhưng chưa kiểm chứng" thì ghi `~` đằng trước.

---

## 0. Con robot này là ai

| Mục | Trả lời |
|---|---|
| Tên | SP01 |
| Nó là một phần của cái gì | |
| Có bao nhiêu con giống hệt nó | |
| Nó làm xong một việc trong bao lâu | |
| Nếu nó ngừng giữa chừng thì hậu quả là gì | |
| Ai đứng cạnh nó khi nó chạy | |

**Một câu duy nhất mô tả nhiệm vụ của nó:**

> 

---

## 1. Giác quan — mỗi tín hiệu vào một dòng

Mỗi đầu vào điền đủ 10 ô. Thiếu ô nào là còn chỗ để hiểu nhầm.

### Mẫu

| Ô | Nghĩa | Ví dụ |
|---|---|---|
| Tên | tên ngữ nghĩa của giác quan | cycle.fill_position |
| Thiết bị | cái gì tạo ra tín hiệu | cảm biến tiệm cận NPN |
| Kiểu | **xung** hay **mức** | xung |
| Nếu xung | rộng bao nhiêu ms, bao lâu lặp lại | ~80 ms, mỗi vòng 1 lần |
| Nếu mức | bật từ lúc nào tới lúc nào | |
| Mức tích cực | ON tương ứng điện áp nào | active-low |
| ON nghĩa là gì | mô tả bằng lời máy, không bằng lời code | vòi đã tới vị trí nhận bao |
| OFF nghĩa là gì | | chưa tới, hoặc đã đi qua |
| Mất tín hiệu thì sao | dây đứt, cảm biến hỏng → máy nên làm gì | |

### Bảng điền

Liệt kê **mọi** giác quan mà máy có, kể cả cái bộ não hiện chưa đọc được. Số lượng
không bị giới hạn bởi số chân của board — chuyện đủ chân hay không là việc của
phần 11.

| Tên | Thiết bị | Kiểu | Xung: rộng/chu kỳ | Mức: từ→đến | Tích cực | ON nghĩa là | OFF nghĩa là | Mất tín hiệu |
|---|---|---|---|---|---|---|---|---|
| | | | | | | | | |
| | | | | | | | | |
| | | | | | | | | |
| | | | | | | | | |
| | | | | | | | | |
| | | | | | | | | |
| | | | | | | | | |
| | | | | | | | | |
| | | | | | | | | |
| | | | | | | | | |

---

## 2. Cân — giác quan đặc biệt

| Mục | Trả lời |
|---|---|
| Thiết bị | |
| Đường truyền, tốc độ | |
| Bao nhiêu lần đọc mỗi giây | |
| Trễ từ lúc xi măng rơi tới lúc số đổi | |
| "Ổn định" nghĩa là gì — bao nhiêu kg trong bao lâu | |
| Cân báo hỏng bằng cách nào | |
| Bao lâu không có số mới thì coi là mất cân | |
| Quy 0 làm lúc nào, ai ra lệnh | |
| Bao rỗng nặng bao nhiêu, có trừ bì không | |
| Xi măng rơi tiếp sau khi đóng van là bao nhiêu kg | |

---

## 3. Cơ bắp — mỗi đầu ra một dòng

| Ô | Nghĩa |
|---|---|
| Tên | tên ngữ nghĩa của cơ bắp |
| Nó điều khiển vật gì | van, contactor, xi lanh… |
| Kiểu | **xung** hay **giữ** |
| Nếu xung | dài bao nhiêu ms |
| Tích cực | |
| Khi mất điện, vật đó ở đâu | **quan trọng nhất**: van đóng hay mở, xi lanh ra hay vào |
| Bật nhầm thì hậu quả gì | |
| Có phản hồi về không | nếu không, ghi rõ "không có" |

| Tên | Điều khiển vật gì | Kiểu | Xung dài | Tích cực | Mất điện thì ở đâu | Bật nhầm thì sao | Phản hồi |
|---|---|---|---|---|---|---|---|
| | | | | | | | |
| | | | | | | | |
| | | | | | | | |
| | | | | | | | |
| | | | | | | | |
| | | | | | | | |
| | | | | | | | |
| | | | | | | | |
| | | | | | | | |

**Cơ cấu nào robot KHÔNG điều khiển nhưng có ảnh hưởng tới nó?**

> 

---

## 4. Hình thể — robot đứng ở đâu trong không gian

| Mục | Trả lời |
|---|---|
| Chiều quay nhìn từ trên | |
| Một vòng mất bao lâu ở tốc độ danh định | |
| Tốc độ có thay đổi không, trong khoảng nào | |
| Gốc 0° quy ước ở đâu | |

### Các mốc góc

| Góc | Sự kiện | Cảm biến | Tên giác quan tương ứng |
|---|---|---|---|
| | | | |
| | | | |
| | | | |
| | | | |
| | | | |

**Sai số cho phép của mỗi mốc là bao nhiêu độ?**

> 

---

## 5. Nhịp — một chu kỳ đầy đủ

Viết bằng lời, như kể chuyện. Mỗi bước ghi: **cái gì kích hoạt** → **robot làm gì** →
**làm sao biết xong**.

```
Bước 1:
  kích hoạt:
  robot làm:
  biết xong khi:
  nếu quá lâu không xong:

Bước 2:
  kích hoạt:
  robot làm:
  biết xong khi:
  nếu quá lâu không xong:

...
```

Số bước bao nhiêu tuỳ máy. Đừng ép vào khuôn có sẵn.

---

## 6. Phản xạ — chuyện bất thường

Mỗi tình huống một khối. Đây là phần quan trọng nhất của cả tài liệu.

```
Tình huống:
  làm sao nhận ra:
  robot phản ứng ngay lập tức:
  sau đó:
  ai xử lý, xử lý thế nào:
  xử lý xong thì robot chạy lại bằng cách nào:
  có được tự động chạy lại không:
```

Những tình huống cần điền, ít nhất:

- bao rách giữa lúc nạp
- tới điểm nhả mà chưa đủ cân
- tới điểm nhả mà băng tải chưa sẵn sàng
- mất tín hiệu cân
- mất tín hiệu vị trí
- mất điều kiện cho phép giữa chừng
- bao kẹt không rời vòi
- máy dừng đột ngột lúc đang nạp
- mất điện rồi có lại

---

## 7. Điều cấm — robot không bao giờ được làm

Viết dạng câu khẳng định tuyệt đối. Đây là những dòng sẽ trở thành kiểm tra trong
code và sẽ chặn mọi thứ khác.

```
1. Không bao giờ ...
2. Không bao giờ ...
3. Không bao giờ ...
```

Và:

| Câu hỏi | Trả lời |
|---|---|
| Nút dừng khẩn nằm ở đâu, cắt cái gì | |
| Nó có đi qua bộ não không, hay cắt cứng | |
| Nếu bộ não treo hẳn, máy dừng bằng cách nào | |
| Nếu bộ não mất điện, máy dừng bằng cách nào | |
| Ai được phép trao quyền điều khiển thật cho robot | |

Một chức năng an toàn đi qua phần mềm thì phụ thuộc phần mềm. Nếu câu trả lời cho
hai dòng giữa là "không có gì khác", hãy ghi đúng như vậy — đó là một phát hiện,
không phải một ô trống.

---

## 8. Trạng thái không thể cùng đúng

Liệt kê mọi cặp tín hiệu không bao giờ được cùng ON, hoặc mọi tổ hợp vô lý.
Đây là thứ để robot tự phát hiện dây sai, cảm biến chết, cấu hình nhầm.

| Tổ hợp | Vì sao không thể | Nghĩa là hỏng cái gì |
|---|---|---|
| | | |
| | | |
| | | |

Và loại thứ hai — vô lý theo thời gian:

| Hiện tượng | Trong bao lâu thì coi là bất thường |
|---|---|
| | |
| | |

---

## 9. Tham số chỉnh được

Cái gì người vận hành đổi được, cái gì chỉ kỹ thuật đổi được, cái gì cố định.

| Tham số | Giá trị hiện dùng | Khoảng cho phép | Ai đổi được | Đổi lúc nào được |
|---|---|---|---|---|
| | | | | |
| | | | | |

---

## 10. Sổ điều chưa biết

Mọi ô `?` ở trên chép xuống đây, kèm cách tìm ra câu trả lời và ai trả lời được.

| Chưa biết | Tìm ra bằng cách nào | Ai biết | Chặn việc gì |
|---|---|---|---|
| | | | |
| | | | |

---

## 11. Đấu nối trên phần cứng hiện tại

Đây là phần **duy nhất** nhắc tới board, chip, số chân. Đổi phần cứng thì chỉ sửa ở
đây.

| Phần cứng đang dùng | |
|---|---|
| Board | |
| Số đầu vào có sẵn | |
| Số đầu ra có sẵn | |
| Đường nối tới cân | |

### Giác quan → chân

| Tên giác quan (phần 1) | Chân | Ghi chú đấu nối |
|---|---|---|
| | | |
| | | |

### Cơ bắp → chân

| Tên cơ bắp (phần 3) | Chân | Ghi chú đấu nối |
|---|---|---|
| | | |
| | | |

### Chưa đủ chân

Giác quan hoặc cơ bắp nào ở phần 1 và 3 **chưa có chân**, và dự định giải quyết ra
sao:

| Tên | Vì sao chưa có chân | Hướng xử lý |
|---|---|---|
| | | |

Gộp nhiều cảm biến vào một chân là một hướng, nhưng phải ghi rõ ở đây cách phân
biệt chúng, vì đó là logic sẽ phải viết trong phần mềm.

---

## Cách dùng bản này

1. Điền phần 1, 2, 3 trước — giác quan và cơ bắp. Đây là sự thật vật lý, đo được,
   không tranh cãi.
2. Phần 4 đo bằng cách quay tay và ghi lại.
3. Phần 5, 6 viết bằng lời, không dùng thuật ngữ phần mềm.
4. Phần 7, 8 hỏi người vận hành lâu năm — họ biết máy hỏng kiểu gì.
5. Phần 11 điền sau cùng. Nếu điền trước, số chân sẵn có sẽ âm thầm giới hạn những
   gì anh nghĩ là máy có.
6. Phần 10 là danh sách việc phải làm.

Khi phần 1–8 không còn ô `?` nào chặn đường, lúc đó mới viết code. Mỗi dòng trong
phần 6 và 7 sẽ thành một test chạy được, và test đó kiểm tra đúng điều anh viết ra
ở đây chứ không phải điều tôi đoán.
