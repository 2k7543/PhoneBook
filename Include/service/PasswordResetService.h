#pragma once
#include <optional>
#include <string>
#include "database/Database.h"
#include "repositories/UserRepository.h"
#include "service/ServiceException.h"
#include "utils/PasswordHasher.h"
using namespace std;

class PasswordResetService {
public:
    // Refactor note: reset tokens are persisted and expiry-checked through
    // the shared user repository/database.
    static constexpr int kTokenMinutes = 15;

    PasswordResetService(Database& db, UserRepository& users, PasswordHasher& hasher);

    optional<string> requestReset(const string& email);
    void resetPassword(const string& token, const string& newPassword);

private:
    Database& db_;
    UserRepository& users_;
    PasswordHasher& hasher_;
};
