#include "service/AdminService.h"
#include "service/PasswordResetService.h"
#include "utils/DateTime.h"
using namespace std;

// Enforces administrator permissions before account-management operations.
AdminService::AdminService(UserRepository& users, ContactRepository& contacts, GroupRepository& groups,
                           AdminLogRepository& logs, PasswordHasher& hasher)
    : users_(users), contacts_(contacts), groups_(groups), logs_(logs), hasher_(hasher) {}

// Verifies the administrator and target account before allowing a sensitive action.
void AdminService::requireAdmin(int adminId) {
    auto u = users_.findById(adminId);
    if (!u || !u->isAdmin() || !u->isActive)
        throw ServiceException(ErrorCode::Forbidden, "Bạn không có quyền quản trị");
}

User AdminService::requireTargetUser(int adminId, int userId) {
    requireAdmin(adminId);
    if (adminId == userId)
        throw ServiceException(ErrorCode::Forbidden, "Không thể thao tác lên chính tài khoản của mình");
    auto target = users_.findById(userId);
    if (!target) throw ServiceException(ErrorCode::NotFound, "Không tìm thấy người dùng");
    if (target->isAdmin())
        throw ServiceException(ErrorCode::Forbidden, "Không thể thao tác lên tài khoản quản trị khác");
    return *target;
}

// Records management actions and provides paginated account listing.
void AdminService::log(int adminId, const string& action, int targetId) {
    logs_.insert(adminId, action, targetId, DateTime::now());
}

vector<User> AdminService::listUsers(int adminId, const string& keyword, int page) {
    requireAdmin(adminId);
    if (page < 1) page = 1;
    return users_.search(keyword, kPageSize, (page - 1) * kPageSize);
}

// Locks, unlocks, resets, or removes a regular account and records the action.
void AdminService::lockUser(int adminId, int userId) {
    requireTargetUser(adminId, userId);
    users_.setActive(userId, false);
    log(adminId, "LOCK_USER", userId);
}

void AdminService::unlockUser(int adminId, int userId) {
    requireTargetUser(adminId, userId);
    users_.setActive(userId, true);
    log(adminId, "UNLOCK_USER", userId);
}

string AdminService::resetUserPassword(int adminId, int userId) {
    requireTargetUser(adminId, userId);
    string temp = PasswordHasher::randomString(12, true);
    users_.updatePassword(userId, hasher_.hash(temp));
    log(adminId, "RESET_PASSWORD", userId);
    return temp;
}

// Issues a short-lived reset token without placing the secret in the audit log.
string AdminService::issuePasswordResetToken(
    int adminId, int userId, PasswordResetService& passwordReset) {
    const User target = requireTargetUser(adminId, userId);
    const auto token = passwordReset.requestReset(target.email);
    if (!token)
        throw ServiceException(ErrorCode::AccountLocked,
                               "Không thể cấp mã cho tài khoản đang bị khóa");
    log(adminId, "ISSUE_PASSWORD_RESET_TOKEN", userId);
    return *token;
}

void AdminService::deleteUser(int adminId, int userId) {
    requireTargetUser(adminId, userId);
    log(adminId, "DELETE_USER", userId);
    users_.remove(userId);
}

// Supplies account counts and aggregate statistics to the admin UI.
int AdminService::countUsers(int adminId) {
    requireAdmin(adminId);
    return users_.countRegularUsers();
}

Statistics AdminService::viewStatistics(int adminId) {
    requireAdmin(adminId);
    Statistics s;
    s.totalUsers = users_.countAll();
    s.activeUsers = users_.countActive();
    s.lockedUsers = s.totalUsers - s.activeUsers;
    s.totalContacts = contacts_.countAll();
    s.totalGroups = groups_.countAll();
    return s;
}
