# Original User Request

## Initial Request — 2026-08-11T14:50:10Z

Xây dựng công cụ TCP Client chạy bằng dòng lệnh (CLI) gọn nhẹ, độc lập (standalone) hoạt động trên môi trường Linux, hỗ trợ cả chế độ tương tác (interactive terminal) và chế độ gửi một lần (one-shot / pipe mode).

Working directory: d:/DEV/3DS/acs_kernel_ncudcntt/tcp-client-cli
Integrity mode: development

## Requirements

### R1. Core TCP Socket & CLI Interface
Công cụ cho phép kết nối tới TCP Server thông qua Host/IP và Port được truyền qua đối số dòng lệnh hoặc tham số (flags). Xử lý mượt mà các trạng thái kết nối, ngắt kết nối, lỗi kết nối và timeout.

### R2. Dual Operating Modes (Interactive & One-Shot/Pipe)
- **Interactive Mode**: Giao diện dòng lệnh tương tác trực tiếp, cho phép người dùng gõ tin nhắn gửi tới server và hiển thị phản hồi từ server theo thời gian thực (real-time).
- **One-Shot / Pipe Mode**: Cho phép truyền dữ liệu từ STDIN (pipe) hoặc tham số dòng lệnh, tự động gửi đến server, nhận phản hồi, in ra STDOUT và kết thúc.

### R3. Lightweight & Standalone Deployment on Linux
Ứng dụng được thiết kế tối giản, biên dịch hoặc đóng gói thành file thực thi độc lập (standalone binary/executable) trên Linux mà không yêu cầu cài đặt thêm các runtime hoặc dependency phức tạp từ bên ngoài.

## Acceptance Criteria

### Functional Verification
- [ ] Có thể kết nối thành công đến bất kỳ TCP Server hợp lệ nào (ví dụ: `nc -l -p <port>`) thông qua tham số `host` và `port`.
- [ ] Ở chế độ Interactive Mode, tin nhắn nhập từ terminal được chuyển tới server và phản hồi từ server hiển thị ngay lập tức lên màn hình.
- [ ] Ở chế độ One-shot / Pipe Mode, lệnh `echo "hello" | ./tcp-client <host> <port>` gửi đúng dữ liệu "hello" và in phản hồi ra STDOUT thành công.
- [ ] Xử lý lỗi cẩn thận: Hiển thị thông báo lỗi rõ ràng khi không kết nối được server, ngắt kết nối đột ngột hoặc địa chỉ/port không hợp lệ (mã thoát exit status khác 0 khi thất bại).

### Build & Portability Verification
- [ ] Chương trình có script/tệp hướng dẫn biên dịch hoặc build tự động ra binary thực thi trên Linux.
- [ ] Kèm theo bộ test tự động (hoặc test script với mock TCP server) để xác minh toàn bộ tính năng hoạt động chính xác.
