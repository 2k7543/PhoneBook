#include "service/AuthService.h"
#include "utils/DateTime.h"
#include "utils/Validator.h"
using namespace std;

// Coordinates account creation, sign-in state, and profile updates.
AuthService::AuthService(UserRepository& users, PasswordHasher& hasher) : users_(users), hasher_(hasher) {}

// Validates and normalizes registration data before saving a hashed password.
User AuthService::registerUser(const string& fullname, const string& username,
                               const string& email, const string& password) {
    string name = Validator::trim(fullname);
    string userName = Validator::toLower(Validator::trim(username));
    string mail = Validator::toLower(Validator::trim(email));

    if (name.empty() || name.size() > 100)
        throw ServiceException(ErrorCode::InvalidInput, "Họ tên phải từ 1 đến 100 ký tự");
    if (!Validator::isValidUsername(userName))
        throw ServiceException(
            ErrorCode::InvalidInput,
            "Tên đăng nhập gồm 2 đến 30 ký tự: chữ, số, dấu chấm, gạch dưới hoặc gạch ngang");
    if (!Validator::isValidEmail(mail))
        throw ServiceException(ErrorCode::InvalidInput, "Email không hợp lệ");
    if (!Validator::isValidPassword(password))
        throw ServiceException(ErrorCode::InvalidInput, "Mật khẩu tối thiểu 8 ký tự, gồm cả chữ và số");
    if (users_.findByEmail(mail))
        throw ServiceException(ErrorCode::AlreadyExists, "Email đã được đăng ký");
    if (users_.findByUsername(userName))
        throw ServiceException(ErrorCode::AlreadyExists, "Tên đăng nhập đã được sử dụng");

    User u;
    u.fullname = name;
    u.username = userName;
    u.email = mail;
    u.passwordHash = hasher_.hash(password);
    u.role = UserRole::User;
    u.isActive = true;
    u.language = "vi";
    u.createDate = DateTime::now();
    u.idUser = users_.insert(u);
    return u;
}

// Clears the prior session, normalizes email, and authenticates an active account.
User AuthService::login(const string& username, const string& password) {
    currentUser_.reset();
    auto user = users_.findByUsername(
        Validator::toLower(Validator::trim(username)));
    if (!user || !hasher_.verify(password, user->passwordHash))
        throw ServiceException(ErrorCode::Unauthorized, "Tên đăng nhập hoặc mật khẩu không đúng");
    if (!user->isActive)
        throw ServiceException(ErrorCode::AccountLocked, "Tài khoản đã bị khóa");
    currentUser_ = *user;
    return *user;
}

// Ends the in-memory session; profile updates keep the session copy in sync.
void AuthService::logout() { currentUser_.reset(); }

void AuthService::updateProfile(int userId, const string& fullname, const string& language,
                                const string& currentPassword, const string& newPassword) {
    auto user = users_.findById(userId);
    if (!user) throw ServiceException(ErrorCode::NotFound, "Không tìm thấy người dùng");

    string name = Validator::trim(fullname);
    if (name.empty() || name.size() > 100)
        throw ServiceException(ErrorCode::InvalidInput, "Họ tên phải từ 1 đến 100 ký tự");
    if (language != "vi" && language != "en")
        throw ServiceException(ErrorCode::InvalidInput, "Ngôn ngữ không được hỗ trợ");

    if (!newPassword.empty()) {
        if (!hasher_.verify(currentPassword, user->passwordHash))
            throw ServiceException(ErrorCode::Unauthorized, "Mật khẩu hiện tại không đúng");
        if (!Validator::isValidPassword(newPassword))
            throw ServiceException(ErrorCode::InvalidInput, "Mật khẩu mới tối thiểu 8 ký tự, gồm cả chữ và số");
        users_.updatePassword(userId, hasher_.hash(newPassword));
    }

    user->fullname = name;
    user->language = language;
    users_.update(*user);
    if (currentUser_ && currentUser_->idUser == userId) currentUser_ = users_.findById(userId);
}
