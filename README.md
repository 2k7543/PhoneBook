# PhoneBook Management

Ứng dụng quản lý danh bạ có giao diện web, REST API C++17 và SQLite. Web server
dùng lại `PhoneBookCore` hiện có; giao diện trình duyệt được viết bằng HTML, CSS
và JavaScript. Mã giao diện Qt desktop cũ vẫn được giữ lại như một lựa chọn build
riêng.

## Build và chạy web app trên máy

Cài CMake, Ninja, Qt 6 (Widgets/SVG), SQLite3, OpenSSL, cpp-httplib và
nlohmann-json. Từ thư mục dự án, build:

```powershell
cmake -S . -B build-qt-runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-qt-runtime --target PhoneBookWebServer --parallel
```

Sau khi build, chạy `RunPhoneBook.bat` để khởi động server và mở trình duyệt tại
[http://localhost:8080](http://localhost:8080). Hoặc chạy trực tiếp:

```powershell
.\build-qt-runtime\PhoneBookWebServer.exe
```

Server lưu database mặc định tại `data\phonebook.db`. Có thể cấu hình đường dẫn
database bằng biến môi trường `PHONEBOOK_DB_PATH` và cổng bằng `PORT`.

## Chức năng web

- Đăng nhập bằng username, đăng ký tài khoản và đặt lại mật khẩu bằng token.
- Thêm, sửa, tìm kiếm, sắp xếp, xóa liên hệ; đánh dấu yêu thích.
- Tạo/xóa nhóm và gán nhóm cho liên hệ.
- Nhập/xuất danh bạ CSV.
- Quản trị viên tìm người dùng, khóa/mở khóa tài khoản và cấp token đặt lại mật khẩu.

## Tài khoản mẫu

Trên database mới:

- Người dùng: `demo` / `12345678`
- Quản trị viên: `admin` / `admin123456`

Đổi mật khẩu mặc định trước khi dùng server ngoài máy cá nhân. Server hiện dùng
HTTP; khi triển khai ra Internet cần HTTPS, quản lý secrets, kiểm soát truy cập
mạng và sao lưu database.

## Kiểm thử và giao diện Qt desktop (tùy chọn)

```powershell
cmake --build build-qt-runtime --parallel
ctest --test-dir build-qt-runtime --output-on-failure
```

Build đầy đủ sẽ tạo cả ứng dụng Qt desktop cũ. Database desktop và web server
cùng dùng `data\phonebook.db` khi chạy từ thư mục dự án.
