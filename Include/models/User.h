#pragma once

#include <string>

// Refactor note: this is the shared user model used by authentication,
// administration, repositories, and the Qt runtime.
enum class UserRole { User, Administrator };

struct User {
    int idUser = 0;
    std::string fullname;
    std::string username;
    std::string email;
    std::string passwordHash;
    UserRole role = UserRole::User;
    bool isActive = true;
    std::string language = "vi";
    std::string createDate;

    bool isAdmin() const noexcept { return role == UserRole::Administrator; }
};
