#pragma once
using namespace std;
#include <string>

class PasswordHasher {
public:
    // Refactor note: member aliases keep all services on the same PBKDF2
    // implementation while retaining their existing service-facing API.
    string hash(const string& password) const { return hashPassword(password); }
    bool verify(const string& password, const string& hashValue) const {
        return verifyPassword(password, hashValue);
    }
    static string hashPassword(const string& password);
    static string hashToken(const string& token);
    static bool verifyPassword(
        const string& password,
        const string& hashedPassword
    );
    static string randomString(size_t length, bool includeDigits = true);
};

