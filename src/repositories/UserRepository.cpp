#include "repositories/UserRepository.h"

// Converts account rows into the domain model and defines a shared column order.
namespace {
User readUser(Statement& query) {
    User user;
    user.idUser = static_cast<int>(query.columnInt(0));
    user.fullname = query.columnText(1);
    user.username = query.columnText(2);
    user.email = query.columnText(3);
    user.passwordHash = query.columnText(4);
    user.role = query.columnText(5) == "admin"
        ? UserRole::Administrator : UserRole::User;
    user.isActive = query.columnBool(6);
    user.language = query.columnText(7);
    user.createDate = query.columnText(8);
    return user;
}

constexpr const char* kUserColumns =
    "id, fullname, username, email, password, role, is_active, language, create_date";
}

// Looks up accounts by ID, username, or email and inserts new account records.
std::optional<User> UserRepository::findById(int id) const {
    auto query = database_.prepare(
        std::string("SELECT ") + kUserColumns + " FROM users WHERE id=?");
    query.bindInt(1, id);
    if (!query.step())
        return std::nullopt;
    return readUser(query);
}

std::optional<User> UserRepository::findByUsername(
    const std::string& username) const {
    auto query = database_.prepare(
        std::string("SELECT ") + kUserColumns +
        " FROM users WHERE lower(username)=lower(?)");
    query.bindText(1, username);
    if (!query.step())
        return std::nullopt;
    return readUser(query);
}

std::optional<User> UserRepository::findByEmail(const std::string& email) const {
    auto query = database_.prepare(
        std::string("SELECT ") + kUserColumns + " FROM users WHERE lower(email)=lower(?)");
    query.bindText(1, email);
    if (!query.step())
        return std::nullopt;
    return readUser(query);
}

int UserRepository::insert(const User& user) {
    auto query = database_.prepare(
        "INSERT INTO users(fullname,username,email,password,role,is_active,language,create_date) "
        "VALUES(?,?,?,?,?,?,?,?)");
    query.bindText(1, user.fullname)
        .bindText(2, user.username)
        .bindText(3, user.email)
        .bindText(4, user.passwordHash)
        .bindText(5, user.isAdmin() ? "admin" : "user")
        .bindInt(6, user.isActive ? 1 : 0)
        .bindText(7, user.language)
        .bindText(8, user.createDate)
        .execute();
    return static_cast<int>(database_.lastInsertId());
}

// Persists profile, password, and active-state changes through focused update methods.
bool UserRepository::update(const User& user) {
    auto query = database_.prepare(
        "UPDATE users SET fullname=?, email=?, role=?, is_active=?, language=? "
        "WHERE id=?");
    query.bindText(1, user.fullname)
        .bindText(2, user.email)
        .bindText(3, user.isAdmin() ? "admin" : "user")
        .bindInt(4, user.isActive ? 1 : 0)
        .bindText(5, user.language)
        .bindInt(6, user.idUser)
        .execute();
    return findById(user.idUser).has_value();
}

void UserRepository::updatePassword(int userId, const std::string& passwordHash) {
    auto query = database_.prepare("UPDATE users SET password=? WHERE id=?");
    query.bindText(1, passwordHash).bindInt(2, userId).execute();
}

void UserRepository::setActive(int userId, bool active) {
    auto query = database_.prepare(
        "UPDATE users SET is_active=? WHERE id=? AND role='user'");
    query.bindInt(1, active ? 1 : 0).bindInt(2, userId).execute();
}

// Deletes a regular account only when the record exists.
bool UserRepository::remove(int userId) {
    if (!findById(userId).has_value())
        return false;
    auto query = database_.prepare("DELETE FROM users WHERE id=? AND role='user'");
    query.bindInt(1, userId).execute();
    return true;
}

// Searches and counts accounts for administration screens.
std::vector<User> UserRepository::search(const std::string& keyword,
                                         int limit, int offset) const {
    auto query = database_.prepare(
        std::string("SELECT ") + kUserColumns +
        " FROM users WHERE role='user' AND "
        "(CAST(id AS TEXT) LIKE ? OR fullname LIKE ? OR username LIKE ? OR email LIKE ?) "
        "ORDER BY id LIMIT ? OFFSET ?");
    const std::string pattern = "%" + keyword + "%";
    query.bindText(1, pattern).bindText(2, pattern).bindText(3, pattern)
        .bindText(4, pattern).bindInt(5, limit).bindInt(6, offset);
    std::vector<User> users;
    while (query.step())
        users.push_back(readUser(query));
    return users;
}

int UserRepository::countAll() const {
    auto query = database_.prepare("SELECT COUNT(*) FROM users");
    query.step();
    return static_cast<int>(query.columnInt(0));
}

int UserRepository::countActive() const {
    auto query = database_.prepare("SELECT COUNT(*) FROM users WHERE is_active=1");
    query.step();
    return static_cast<int>(query.columnInt(0));
}

int UserRepository::countRegularUsers() const {
    auto query = database_.prepare("SELECT COUNT(*) FROM users WHERE role='user'");
    query.step();
    return static_cast<int>(query.columnInt(0));
}

// Persists one-time reset-token metadata and invalidates older outstanding tokens.
void UserRepository::saveResetToken(int userId, const std::string& token,
                                    const std::string& expiresAt) {
    auto invalidate = database_.prepare(
        "UPDATE password_reset_tokens SET used=1 WHERE user_id=? AND used=0");
    invalidate.bindInt(1, userId).execute();
    auto query = database_.prepare(
        "INSERT INTO password_reset_tokens(token,user_id,expires_at,used) "
        "VALUES(?,?,?,0)");
    query.bindText(1, token).bindInt(2, userId).bindText(3, expiresAt).execute();
}

std::optional<ResetTokenInfo> UserRepository::findResetToken(
    const std::string& token) const {
    auto query = database_.prepare(
        "SELECT user_id,expires_at,used FROM password_reset_tokens WHERE token=?");
    query.bindText(1, token);
    if (!query.step())
        return std::nullopt;
    return ResetTokenInfo{
        static_cast<int>(query.columnInt(0)),
        query.columnText(1),
        query.columnBool(2)
    };
}

void UserRepository::markResetTokenUsed(const std::string& token) {
    auto query = database_.prepare(
        "UPDATE password_reset_tokens SET used=1 WHERE token=?");
    query.bindText(1, token).execute();
}

// Returns all account rows so startup can migrate legacy password values.
std::vector<User> UserRepository::allForPasswordMigration() const {
    auto query = database_.prepare(
        std::string("SELECT ") + kUserColumns + " FROM users");
    std::vector<User> users;
    while (query.step())
        users.push_back(readUser(query));
    return users;
}
