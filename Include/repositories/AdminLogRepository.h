#pragma once

#include <string>
#include <vector>

#include "database/Database.h"

struct AdminLog {
    int id = 0;
    int adminId = 0;
    int targetUserId = 0;
    std::string action;
    std::string description;
    std::string timestamp;
};

// Refactor note: audit events are written to SQLite instead of a temporary
// vector so administrative actions remain reviewable.
class AdminLogRepository {
public:
    explicit AdminLogRepository(Database& database) : database_(database) {}

    void insert(int adminId, const std::string& action, int targetUserId,
                const std::string& timestamp);
    std::vector<AdminLog> getAll() const;
    std::vector<AdminLog> findByAdminId(int adminId) const;
    bool remove(int id);

private:
    Database& database_;
};
