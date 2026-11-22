#pragma once

#include <optional>
#include <string>
#include <vector>

#include "database/Database.h"
#include "models/User.h"

struct ResetTokenInfo {
    int userId = 0;
    std::string expiresAt;
    bool used = false;
};

// Refactor note: replaces the old process-local vector with parameterized
// SQLite queries so account state persists across application runs.
class UserRepository {
public:
    explicit UserRepository(Database& database) : database_(database) {}

    std::optional<User> findById(int id) const;
    std::optional<User> findByUsername(const std::string& username) const;
    std::optional<User> findByEmail(const std::string& email) const;
    int insert(const User& user);
    bool update(const User& user);
    void updatePassword(int userId, const std::string& passwordHash);
    void setActive(int userId, bool active);
    bool remove(int userId);
    std::vector<User> search(const std::string& keyword, int limit, int offset) const;
    int countAll() const;
    int countActive() const;
    int countRegularUsers() const;
    void saveResetToken(int userId, const std::string& token,
                        const std::string& expiresAt);
    std::optional<ResetTokenInfo> findResetToken(const std::string& token) const;
    void markResetTokenUsed(const std::string& token);
    std::vector<User> allForPasswordMigration() const;

private:
    Database& database_;
};
