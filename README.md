# NiceKey

[Tải NiceKey 1.0.0 cho macOS (.dmg)](https://github.com/klee3721/NiceKey/releases/download/v1.0.0/NiceKey-1.0.0.dmg)

NiceKey là bộ gõ tiếng Việt nguồn mở cho macOS, được phát triển từ nền tảng OpenKey. Mục tiêu của NiceKey là giữ lại phần lõi gõ tiếng Việt ổn định, nhẹ và quen thuộc, đồng thời tách thành một nhánh phát triển riêng với trải nghiệm cài đặt, nhận diện app và cấu hình phát hành rõ ràng hơn.

NiceKey vẫn tuân thủ GPL-3.0 và giữ đầy đủ ghi nhận đối với tác giả, contributor của OpenKey gốc.

Liên hệ phát triển: Telegram [@kienvu37](https://t.me/kienvu37)

## Demo

<p align="center">
  <img src="assets/demo-main.png" alt="Cua so cau hinh NiceKey" width="560">
</p>

<p align="center">
  <img src="assets/demo-menu.png" alt="Menu NiceKey tren thanh trang thai macOS" width="280">
</p>

## Cải tiến so với OpenKey gốc

- Tách nhận diện thành NiceKey với bundle id riêng `com.nicekey.NiceKey`, tránh lẫn với OpenKey khi cài đặt, cấp quyền và phát triển tiếp.
- Làm lại luồng cấp quyền lần đầu: app chỉ mở bảng cấp quyền Accessibility, sau khi người dùng cấp quyền sẽ thông báo, tự thoát và mở lại để kích hoạt bộ gõ sạch từ đầu.
- Bổ sung lịch sử Clipboard từ mkey, có popup danh sách clipboard, cấu hình phím tắt và giới hạn 30 mục gần nhất.
- Dọn lại phần giới thiệu, liên hệ, release và thông tin giấy phép cho nhánh NiceKey.
- Đóng gói DMG kiểu kéo thả đơn giản: mở file cài chỉ thấy `NiceKey.app` và `Applications`.
- Tắt các liên kết cá nhân/donation của dự án gốc trong tài liệu chính, đồng thời vẫn giữ giấy phép và ghi nhận nguồn mở đầy đủ.

## Cài đặt

Tải file DMG mới nhất:

[NiceKey-1.0.0.dmg](https://github.com/klee3721/NiceKey/releases/download/v1.0.0/NiceKey-1.0.0.dmg)

Mở file `.dmg`, kéo `NiceKey.app` vào thư mục `Applications`, sau đó mở NiceKey từ Applications.

## Build macOS

Mở project bằng Xcode:

```bash
open Sources/OpenKey/macOS/OpenKey.xcodeproj
```

Hoặc build từ terminal:

```bash
xcodebuild -project Sources/OpenKey/macOS/OpenKey.xcodeproj -scheme NiceKey -configuration Debug build
```

Nếu build trên máy chưa cấu hình signing, có thể dùng:

```bash
xcodebuild -project Sources/OpenKey/macOS/OpenKey.xcodeproj -scheme NiceKey -configuration Debug CODE_SIGNING_ALLOWED=NO build
```

## Cấp quyền macOS

NiceKey cần quyền Accessibility để bắt và xử lý phím. Lần đầu mở app, NiceKey sẽ chỉ hiển thị bảng cấp quyền. Sau khi cấp quyền, app sẽ thông báo, tự thoát và mở lại để kích hoạt bộ gõ.

## Giấy phép và ghi nhận

NiceKey tiếp tục phát hành theo GPL-3.0. Xem [LICENSE](LICENSE) và [NOTICE.md](NOTICE.md).

Source gốc là OpenKey. Các copyright notice của tác giả gốc và contributor vẫn được giữ trong source để tuân thủ giấy phép nguồn mở.
