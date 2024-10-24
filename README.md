# Assignment_4-IoT
Chỉnh sửa ví dụ Broadcast:
Node sink/root tạo beacon theo chu kỳ 8s và quảng bá cho các nút lân cận
Các nút lân cận khi nhận được beacon sẽ update bảng neighbor: RSSI, PRR, RX_counter.
Sau đó sắp xếp lại bảng neighbor theo RSSI giảm dần.
Bảng neighbor chỉ lưu được thông tin của 5 nút lân cận
Mô phỏng 1 mạng đa chặng, trong đó 1 nút sẽ có 8-10 nút lân cận.
