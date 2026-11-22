#pragma once
#include <optional>
#include <string>
#include "models/User.h"
#include "repositories/UserRepository.h"
#include "service/ServiceException.h"
#include "utils/PasswordHasher.h"
using namespace std;

class AuthService {
public:
    // Refactor note: the authentication layer now consumes database-backed
    // users and provides one login/register path to the Qt application.
    AuthService(UserRepository& users, PasswordHasher& hasher);

    User registerUser(const string& fullname, const string& username,
                      const string& email, const string& password);
    User login(const string& username, const string& password);
    void logout();

    void updateProfile(int userId, const string& fullname, const string& language,
                       const string& currentPassword = "", const string& newPassword = "");

    bool isLoggedIn() const { return currentUser_.has_value(); }
    const optional<User>& currentUser() const { return currentUser_; }

private:
    UserRepository& users_;
    PasswordHasher& hasher_;
    optional<User> currentUser_;
};
