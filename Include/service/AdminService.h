#pragma once
#include <string>
#include <vector>
#include "models/User.h"
#include "repositories/AdminLogRepository.h"
#include "repositories/ContactRepository.h"
#include "repositories/GroupRepository.h"
#include "repositories/UserRepository.h"
#include "service/ServiceException.h"
#include "utils/PasswordHasher.h"
using namespace std;

class PasswordResetService;

struct Statistics {
    int totalUsers = 0;
    int activeUsers = 0;
    int lockedUsers = 0;
    int totalContacts = 0;
    int totalGroups = 0;
};

class AdminService {
public:
    // Refactor note: admin permissions, account actions, and audit events
    // share one database-backed service contract.
    static constexpr int kPageSize = 20;

    AdminService(UserRepository& users, ContactRepository& contacts, GroupRepository& groups,
                 AdminLogRepository& logs, PasswordHasher& hasher);

    vector<User> listUsers(int adminId, const string& keyword, int page);
    void lockUser(int adminId, int userId);
    void unlockUser(int adminId, int userId);
    string resetUserPassword(int adminId, int userId);
    string issuePasswordResetToken(int adminId, int userId,
                                  PasswordResetService& passwordReset);
    void deleteUser(int adminId, int userId);
    Statistics viewStatistics(int adminId);
    int countUsers(int adminId);

private:
    UserRepository& users_;
    ContactRepository& contacts_;
    GroupRepository& groups_;
    AdminLogRepository& logs_;
    PasswordHasher& hasher_;

    void requireAdmin(int adminId);
    User requireTargetUser(int adminId, int userId);
    void log(int adminId, const string& action, int targetId);
};
