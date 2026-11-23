# PhoneBook Management

Ứng dụng quản lý danh bạ có giao diện web, REST API C++17 và SQLite. Web server
dùng lại `PhoneBookCore` hiện có; giao diện trình duyệt được viết bằng HTML, CSS
và JavaScript. Mã giao diện Qt desktop cũ vẫn được giữ lại như một lựa chọn build
riêng.

## Build và chạy web app trên máy

Cài CMake, Ninja, Qt 6 (Widgets/SVG), SQLite3, OpenSSL, libcurl, cpp-httplib và
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

## Đặt lại mật khẩu qua email

Người dùng chọn **Quên mật khẩu?**, nhập email đã đăng ký, rồi dùng mã gửi đến
hộp thư để đặt mật khẩu mới. Mã chỉ dùng một lần, được lưu dạng băm và hết hạn
sau 15 phút. Phản hồi không tiết lộ email có tài khoản hay không.

Để gửi email, cấu hình SMTP bằng các biến môi trường sau (không ghi mật khẩu vào
mã nguồn):

- `PHONEBOOK_SMTP_URL`: URL SMTP, ví dụ `smtp://smtp.gmail.com:587` (STARTTLS).
- `PHONEBOOK_SMTP_USERNAME`: tài khoản SMTP.
- `PHONEBOOK_SMTP_PASSWORD`: mật khẩu SMTP hoặc app password.
- `PHONEBOOK_SMTP_FROM`: địa chỉ gửi email.

Với Gmail, hãy bật xác minh 2 bước và tạo **Google App Password**; không dùng
mật khẩu đăng nhập Gmail thông thường. Sao chép `.env.example` thành `.env` rồi
điền thông tin SMTP thật. Không gửi hoặc commit file `.env`; file này được loại
khỏi Git và Docker build context.

Khi chạy bằng Docker, build lại image và tạo lại container để nạp cấu hình:

```powershell
docker build -t phonebook-web .
docker rm -f phonebook-web
docker run -d --name phonebook-web -p 127.0.0.1:8081:8080 --mount source=phonebookmanagement_phonebook_data,target=/app/data --env-file .env phonebook-web
```

Volume database vẫn được giữ khi xóa container. Nếu SMTP chưa cấu hình, API
trả lỗi cấu hình rõ ràng; lỗi gửi email được ghi vào log server.

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
