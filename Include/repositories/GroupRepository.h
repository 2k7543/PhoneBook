#pragma once

#include <optional>
#include <string>
#include <vector>

#include "database/Database.h"
#include "models/ContactGroup.h"

// Refactor note: group persistence is scoped by owner ID and shared by the
// sidebar, detail selector, and group services.
class GroupRepository {
public:
    explicit GroupRepository(Database& database) : database_(database) {}

    std::optional<ContactGroup> findById(int groupId, int userId) const;
    std::vector<ContactGroup> findByUser(int userId) const;
    int insert(const ContactGroup& group);
    bool existsName(int userId, const std::string& name) const;
    void rename(int groupId, int userId, const std::string& name);
    bool remove(int groupId, int userId);
    void addContact(int groupId, int contactId);
    bool removeContact(int groupId, int contactId);
    int countAll() const;

private:
    Database& database_;
};
