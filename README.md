# PhoneBook Management

Ứng dụng quản lý danh bạ desktop viết bằng C++17, Qt 6 và SQLite.

## Build

```powershell
cmake -S . -B build-qt-runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-qt-runtime --parallel
ctest --test-dir build-qt-runtime --output-on-failure
```

The build requires Qt 6 (Widgets, SVG), SQLite3, OpenSSL, and Ninja. The Qt
application links `PhoneBookCore`, which builds the domain models, SQLite
repositories, services, database initializer, validation, date, and password
utilities from `Include` and `src`. Authentication, contact listing/search,
CRUD, favorites, groups, CSV, account administration, audit logging, and reset
tokens use that service graph. Qt is the presentation layer and no longer owns
SQL queries.

## Chạy ứng dụng

Chạy file `RunPhoneBook.bat` ở thư mục gốc của dự án để mở ứng dụng. File này
tự dùng bản build trong `build-qt-runtime` và cấu hình Qt runtime.

Để chạy trực tiếp từ PowerShell:

```powershell
set PATH=C:\msys64\ucrt64\bin;%PATH%
build-qt-runtime\PhoneBookManagementQt.exe
```

Đăng nhập bằng **tên đăng nhập** (không dùng email):

- Tài khoản demo: `demo`
- Mật khẩu: `12345678`

Tài khoản quản trị:

- Tên đăng nhập: `admin`
- Mật khẩu: `admin123456`

Năm tài khoản mẫu khác dùng mật khẩu `12345678`:
`an`, `binh`, `cuong`, `dao`, và `minh`.

Người dùng mới có thể chọn **Tạo tài khoản** ở màn hình đăng nhập. Cần nhập họ
tên, tên đăng nhập duy nhất (2–30 ký tự: chữ, số, dấu chấm, gạch dưới hoặc gạch
ngang), email hợp lệ và mật khẩu tối thiểu 8 ký tự gồm chữ và số.

Cơ sở dữ liệu SQLite được lưu tại `data\phonebook.db`.

Quên mật khẩu: admin chọn **Mã** ở dòng tài khoản để tạo token dùng một lần,
hạn 15 phút, rồi gửi riêng token cho người dùng. Người dùng chọn **Quên mật khẩu?**
ở màn hình đăng nhập, nhập token và mật khẩu mới. Ứng dụng chưa gửi email khôi
phục tự động; token không được ghi vào AdminLog.
