#include "repositories/AdminLogRepository.h"

// Converts a database row into the audit-log model.
namespace {
AdminLog readLog(Statement& query) {
    return AdminLog{
        static_cast<int>(query.columnInt(0)),
        static_cast<int>(query.columnInt(1)),
        static_cast<int>(query.columnInt(2)),
        query.columnText(3),
        query.columnText(4),
        query.columnText(5)
    };
}
}

// Persists an administrator action without storing account credentials.
void AdminLogRepository::insert(int adminId, const std::string& action,
                                int targetUserId,
                                const std::string& timestamp) {
    auto query = database_.prepare(
        "INSERT INTO AdminLog(admin_id,target_user_id,action,detail,created_at) "
        "VALUES(?,?,?,?,?)");
    query.bindInt(1, adminId)
        .bindInt(2, targetUserId)
        .bindText(3, action)
        .bindText(4, "target_user_id=" + std::to_string(targetUserId))
        .bindText(5, timestamp)
        .execute();
}

// Reads audit records in newest-first order, globally or for one administrator.
std::vector<AdminLog> AdminLogRepository::getAll() const {
    auto query = database_.prepare(
        "SELECT id,admin_id,target_user_id,action,detail,created_at "
        "FROM AdminLog ORDER BY id DESC");
    std::vector<AdminLog> logs;
    while (query.step())
        logs.push_back(readLog(query));
    return logs;
}

std::vector<AdminLog> AdminLogRepository::findByAdminId(int adminId) const {
    auto query = database_.prepare(
        "SELECT id,admin_id,target_user_id,action,detail,created_at "
        "FROM AdminLog WHERE admin_id=? ORDER BY id DESC");
    query.bindInt(1, adminId);
    std::vector<AdminLog> logs;
    while (query.step())
        logs.push_back(readLog(query));
    return logs;
}

// Removes a record only when it exists.
bool AdminLogRepository::remove(int id) {
    auto exists = database_.prepare("SELECT 1 FROM AdminLog WHERE id=?");
    exists.bindInt(1, id);
    if (!exists.step())
        return false;
    auto query = database_.prepare("DELETE FROM AdminLog WHERE id=?");
    query.bindInt(1, id).execute();
    return true;
}
