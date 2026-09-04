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

## Follow-up — 2026-09-04T03:26:44Z

This is a single self-contained fix; keep it small and focused.

Khắc phục triệt để các lỗi regression trong bộ phân tích tham số dòng lệnh (CLI parser) của `tcp-client`, phục hồi đầy đủ tính năng của các cờ (`-h`, `-a`, `-T`, `-L`, `-D`), và tinh chỉnh tính tương thích trên Windows/Linux để vượt qua 100% các bài kiểm thử tự động.

Working directory: d:/DEV/3DS/acs_kernel_ncudcntt/tcp-client-cli
Integrity mode: development

## Requirements

### R1. Phục hồi toàn vẹn bộ phân tích CLI trong `src/cli_args.c`
Khắc phục các khiếm khuyết trong logic phân tích tham số:
- Khôi phục xử lý cờ máy chủ (`-h` / `--host`).
- Tách biệt hoàn toàn xử lý chuỗi ASCII (`-a` / `--ascii`) và ký tự kết thúc (`-T` / `--term`), ngăn chặn lỗi nhận diện nhầm tham số.
- Phục hồi việc nhận diện cờ tiền tố độ dài TCP 2-byte (`-L` / `--add-tcp-len`) và bộ giải mã HSM (`-D` / `--decode-hsm`).
- Đồng bộ thông điệp trợ giúp (`print_usage`) và danh sách `long_options` để phản ánh đúng và đủ tất cả các tùy chọn CLI hiện có.

### R2. Đảm bảo tính ổn định và tương thích đa nền tảng (Windows & POSIX)
Xử lý các trường hợp đóng kết nối đột ngột, timeout tức thì (1ms) và quản lý stream I/O trên Windows để toàn bộ các kịch bản tương tác và one-shot đạt tính tất định cao nhất.

## Verification Resources
- Test suite tự động: `python tests/test_runner.py` (chứa 65 test cases đa tầng từ T1 đến T5).
- Mã nguồn kiểm thử mock server: `tests/mock_server.py`.

## Acceptance Criteria

### CLI Parsing & Functional Behavior
- [ ] Lệnh `./tcp-client -h 127.0.0.1 -p <port>` và `./tcp-client --host 127.0.0.1 --port <port>` kết nối thành công, không trả về mã lỗi 1.
- [ ] Cờ `-a` hỗ trợ đầy đủ escape sequence (ví dụ: `\x19`, `\r\n`) mà không bị xung đột với `-T`.
- [ ] Cờ `-T <hex>` nối đúng 1 byte vào cuối dữ liệu gửi và ngắt đọc ngay khi nhận delimiter từ server.
- [ ] Cờ `-L` chèn đúng 2-byte Big-Endian length header vào trước payload.
- [ ] Cờ `-D` kích hoạt đúng bộ giải mã HSM Thales payShield 10K.
- [ ] Lệnh `./tcp-client --help` hiển thị đầy đủ thông tin về `-h`, `-p`, `-t`, `-x`, `-X`, `-a`, `-T`, `-L`, `-D`, `-i`, `-v`, `-H`, `-V`.

### Automated Test Suite
- [ ] Chạy `python tests/test_runner.py` vượt qua toàn bộ 65/65 test case (0 failure).

