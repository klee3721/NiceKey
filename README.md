# NiceKey

NiceKey là bộ gõ tiếng Việt nguồn mở cho macOS, được fork từ OpenKey để tiếp tục phát triển theo hướng riêng. Dự án giữ nguyên tinh thần phần mềm tự do, tuân thủ GPL-3.0 và bảo toàn ghi nhận đối với tác giả, contributor của OpenKey.

- Repository: https://github.com/klee3721/NiceKey
- Liên hệ phát triển: Telegram [@kienvu37](https://t.me/kienvu37)

## Trạng thái hiện tại

- App macOS đã được đổi tên hiển thị thành NiceKey.
- Bundle id macOS đã đổi sang `com.nicekey.NiceKey`.
- Version fork bắt đầu tại `1.0.0` build `1`.
- Link repository, release và liên hệ đã được cấu hình cho NiceKey.
- Các link cá nhân, donation, fanpage và email của dự án gốc đã được gỡ khỏi tài liệu chính.

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
