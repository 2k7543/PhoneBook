#pragma once

#include <optional>
#include <string>
#include <vector>

#include "database/Database.h"
#include "models/Contact.h"
#include "repositories/AdminLogRepository.h"
#include "repositories/ContactRepository.h"
#include "repositories/GroupRepository.h"
#include "repositories/UserRepository.h"
#include "service/AdminService.h"
#include "service/AuthService.h"
#include "service/ContactService.h"
#include "service/CsvService.h"
#include "service/GroupService.h"
#include "service/PasswordResetService.h"
#include "utils/PasswordHasher.h"

struct RuntimeUser {
    int id = 0;
    std::string name;
    bool active = false;
};

struct RuntimeSession {
    int userId = 0;
    std::string name;
    bool admin = false;
};

struct RuntimeContact {
    std::string lastName;
    std::string firstName;
    std::string phone;
    std::string email;
    std::string company;
    std::string address;
    std::string note;
};

struct RuntimeGroup {
    int id = 0;
    std::string name;
};

// Refactor note: this is the composition root used by Qt; all domain actions
// delegate to services and repositories instead of embedding SQL in widgets.
class PhoneBookRuntime {
public:
    explicit PhoneBookRuntime(const std::string& databasePath);

    void migratePasswords();
    int registerUser(const std::string& fullname, const std::string& username,
                     const std::string& email, const std::string& password);
    RuntimeSession authenticate(const std::string& username,
                               const std::string& password);
    std::vector<RuntimeUser> listUsers(int adminId,
                                       const std::string& keyword);
    int countUsers(int adminId);
    void setUserActive(int adminId, int userId, bool active);

    int addContact(int userId, const RuntimeContact& contact);
    void updateContact(int userId, int contactId, const RuntimeContact& contact);
    void removeContact(int userId, int contactId);
    bool toggleFavorite(int userId, int contactId);
    std::vector<Contact> listContacts(int userId, const std::string& keyword,
                                      int groupId, bool favoritesOnly,
                                      ContactSort sort) const;
    int createGroup(int userId, const std::string& name);
    void removeGroup(int userId, int groupId);
    void setContactGroup(int userId, int contactId, int groupId);
    std::vector<RuntimeGroup> listGroups(int userId) const;
    std::vector<int> contactGroupIds(int userId, int contactId) const;

    ImportResult importContacts(int userId, const std::string& filePath);
    int exportContacts(int userId, const std::string& filePath) const;
    std::optional<std::string> requestPasswordReset(const std::string& email);
    void resetPassword(const std::string& token, const std::string& newPassword);
    std::string issuePasswordResetToken(int adminId, int userId);

private:
    Database database_;
    PasswordHasher passwordHasher_;
    UserRepository users_;
    ContactRepository contacts_;
    GroupRepository groups_;
    AdminLogRepository adminLogs_;
    AuthService authService_;
    ContactService contactService_;
    GroupService groupService_;
    AdminService adminService_;
    CsvService csvService_;
    PasswordResetService passwordResetService_;

    static Contact toContact(int userId, const RuntimeContact& input);
    void seedDefaultAccountsAndContacts();
};
