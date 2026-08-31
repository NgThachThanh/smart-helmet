# Smart Helmet — ESP32-S3

Mũ bảo hiểm thông minh cho người đi mô tô dựa trên ESP32-S3 và FreeRTOS, tích hợp **phát hiện té ngã đa bằng chứng**, **HUD hiển thị điều hướng qua BLE** và **cảnh báo SOS tự động** kèm vị trí GPS.

## Thành tích

- Đề tài nghiên cứu khoa học cấp trường
- Top 20 — Cuộc thi BK Innovation 2025, Đại học Bách khoa TP.HCM
- Bài báo được chấp nhận tại hội nghị CUTE2026: *Toward a Hybrid Efficient Edge-Host Smart Helmet System for Drowsiness Detection and Accident Verification using ESP32-S3*

![Kiến trúc hệ thống](docs/overview.png)

## Tính năng

| Tính năng | Mô tả |
|---|---|
| Phát hiện té ngã đa bằng chứng | MPU6050 ở 100 Hz, lọc Kalman + complementary, máy trạng thái 3 tầng (va chạm/nghiêng → cửa sổ hồi phục 3 s → đếm ngược 10 s) |
| Cảnh báo SOS | Không huỷ trong 10 s → tự động gửi SMS kèm link Google Maps tới người thân qua SIM A7680C (4G) + định vị ATGM336H |
| HUD điều hướng | OLED 0.96" (SSD1306) hiển thị hướng dẫn từng chặng từ app React Native qua BLE — tên đường, khoảng cách, icon rẽ trái/phải |
| Phát hiện buồn ngủ | YOLOv5n chạy trên Sipeed MaixCAM (edge AI), dataset 15.000 ảnh góc nghiêng tự thu thập, lọc thời gian theo độ dài nhắm mắt |
| Tiết kiệm năng lượng | Pin 2000 mAh dùng 7–9 giờ, tổng chi phí hệ thống dưới 85 USD |

## Phần cứng

| Linh kiện | Vai trò |
|---|---|
| ESP32-S3 (DevKitC-1) | MCU chính — đọc IMU, BLE, HUD, FreeRTOS đa nhân |
| MPU6050 | IMU 6 trục phát hiện té ngã (I2C) |
| SIM A7680C | Module 4G gửi SMS SOS (UART2) |
| ATGM336H | Module GPS, xuất NMEA qua UART1 |
| OLED 0.96" SSD1306 | Màn hình HUD (I2C) |
| Sipeed MaixCAM | Suy luận thị giác — phát hiện buồn ngủ (không nằm trong repo này) |

## Cách hoạt động

### Phát hiện té ngã (3 tầng)

1. **Tầng 1 — Kích hoạt**: tổng gia tốc vượt ~2,5 g (ngưỡng nhân hệ số tốc độ/đường/cách lái) **hoặc** mũ nghiêng quá 60° so với phương thẳng đứng.
2. **Tầng 2 — Cửa sổ hồi phục (3 s)**: nếu tư thế trở lại (nghiêng dưới 30°, gia tốc ổn định 0,8–1,2 g) → sự kiện bị loại là báo động giả (phanh gấp, va ổ gà).
3. **Tầng 3 — Cảnh báo (đếm ngược 10 s)**: LED nhấp nháy, người lái giữ nút huỷ để bỏ qua. Hết giờ: task SMS được đánh thức — chờ GPS fix rồi gửi SMS chứa `https://maps.google.com/?q=<lat>,<lon>`, sau đó tạm dừng chờ sự kiện kế tiếp.

![Thuật toán phát hiện té ngã](docs/sos.png)

### HUD + điều hướng BLE

Mũ quảng cáo BLE service `DD3F0AD1-6239-4E1F-81F1-91F6C9F01D86`. App React Native ghi payload JSON (`nav`, `inst`, `dist`, `street`, `nstreet`, `dir`, `step`, `total`) vào write characteristic; ESP32 parse và vẽ lên OLED. Dấu tiếng Việt được lược bỏ để hiển thị với font cơ bản của SSD1306.

![HUD](docs/HUD.png)

## Sơ đồ đấu nối

| Chân ESP32-S3 | Ngoại vi |
|---|---|
| 33 (SDA) / 32 (SCL) | MPU6050 + OLED (I2C) |
| 18 (RX) / 17 (TX) | GPS ATGM336H (UART1, 9600) |
| 26 (RX) / 27 (TX) | SIM A7680C (UART2, 115200) |
| 2 | LED cảnh báo |
| 4 | Nút huỷ cảnh báo (INPUT_PULLUP, LOW = nhấn) |

> Số điện thoại nhận SOS cấu hình trong `firmware/src/SmsGps.h` → `SOS_PHONE`.

## Cấu trúc repo

```
firmware/
├── platformio.ini       # cấu hình build + thư viện
└── src/
    ├── main.ino         # FreeRTOS: task Sensor (100 Hz) / HUD / SMS
    ├── VapCo.h/.cpp     # SmartHelmetSensor — máy trạng thái phát hiện té ngã
    ├── HUD.h/.cpp       # OLED + BLE server + parser JSON điều hướng
    └── SmsGps.h/.cpp    # Parse NMEA GPS + lệnh AT SIM A7680C
docs/                    # ảnh kiến trúc và prototype
```

## Kết quả (thử nghiệm thực tế tại TP. Hồ Chí Minh)

- Giảm 35% thời gian mắt rời đường (giao thức ISO 15007 A/B, so với điện thoại gắn ghi đông)
- Tỉ lệ báo động giả 8%
- Độ chính xác nhận trạng thái mắt 78% ở 30 FPS trên MaixCAM (YOLOv5n, dataset tự thu thập)
- Thời gian từ phát hiện té đến gửi SMS trung bình 12 s; SMS thành công 95% ngay cả vùng sóng yếu
- Pin 7–9 giờ; tổng chi phí hệ thống dưới 85 USD

## License

MIT — xem [LICENSE](LICENSE).
