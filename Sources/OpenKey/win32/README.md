# NiceKey for Windows

Thư mục này chứa bản NiceKey cho Windows, sử dụng lõi gõ tiếng Việt kế thừa từ OpenKey và giao diện Win32 native.

## Tính năng

- Bộ gõ Telex, VNI, Simple Telex và các bảng mã có sẵn từ lõi OpenKey.
- Gõ tắt, chuyển mã, ghi nhớ chế độ theo ứng dụng và khởi động cùng Windows.
- Lịch sử Clipboard tối đa 30 mục, hỗ trợ văn bản và ảnh, tìm kiếm không dấu, chọn bằng chuột hoặc bàn phím, ghim và tự ẩn.
- Phím tắt lịch sử Clipboard có thể cấu hình; mặc định là `Ctrl+Shift+V`.
- Dữ liệu và cấu hình riêng trong `%LOCALAPPDATA%\NiceKey` và `HKCU\Software\NiceKey`.

## Build

Mở `OpenKey/NiceKey.sln` bằng Visual Studio 2022, dùng toolset v143 và Windows 10 SDK. Build cấu hình `Release|x64` để tạo `NiceKey64.exe` hoặc `Release|x86` để tạo `NiceKey32.exe`.

Workflow `.github/workflows/windows-release.yml` tự build hai kiến trúc, chạy `--self-test`, tạo bản portable và bộ cài Inno Setup.

## Giấy phép

Mã nguồn tiếp tục phát hành theo GPL-3.0. Xem `LICENSE` ở thư mục gốc và `NOTICE.md` để biết thông tin ghi nhận source gốc.
