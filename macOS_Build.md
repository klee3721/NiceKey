# Build NiceKey cho macOS

Yêu cầu:

- macOS Mojave trở lên.
- Xcode 10 trở lên.

Mở project:

```bash
open Sources/OpenKey/macOS/OpenKey.xcodeproj
```

Trong Xcode, chọn scheme `NiceKey`, sau đó dùng `Product > Build`, `Product > Run` hoặc `Product > Archive`.

Build bằng terminal:

```bash
xcodebuild -project Sources/OpenKey/macOS/OpenKey.xcodeproj -scheme NiceKey -configuration Debug build
```

Nếu máy chưa cấu hình certificate signing:

```bash
xcodebuild -project Sources/OpenKey/macOS/OpenKey.xcodeproj -scheme NiceKey -configuration Debug CODE_SIGNING_ALLOWED=NO build
```

Output mặc định sẽ là `NiceKey.app`.
