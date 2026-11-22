#include "repositories/GroupRepository.h"

// Group queries always include the owner ID to keep address books isolated.
std::optional<ContactGroup> GroupRepository::findById(
    int groupId, int userId) const {
    auto query = database_.prepare(
        "SELECT id,user_id,name FROM contact_groups WHERE id=? AND user_id=?");
    query.bindInt(1, groupId).bindInt(2, userId);
    if (!query.step())
        return std::nullopt;
    return ContactGroup{
        static_cast<int>(query.columnInt(0)),
        static_cast<int>(query.columnInt(1)),
        query.columnText(2)
    };
}

// Lists, creates, renames, and removes groups for a single user.
std::vector<ContactGroup> GroupRepository::findByUser(int userId) const {
    auto query = database_.prepare(
        "SELECT id,user_id,name FROM contact_groups WHERE user_id=? "
        "ORDER BY name COLLATE NOCASE");
    query.bindInt(1, userId);
    std::vector<ContactGroup> groups;
    while (query.step()) {
        groups.push_back({
            static_cast<int>(query.columnInt(0)),
            static_cast<int>(query.columnInt(1)),
            query.columnText(2)
        });
    }
    return groups;
}

int GroupRepository::insert(const ContactGroup& group) {
    auto query = database_.prepare(
        "INSERT INTO contact_groups(user_id,name) VALUES(?,?)");
    query.bindInt(1, group.userId).bindText(2, group.name).execute();
    return static_cast<int>(database_.lastInsertId());
}

// Checks group-name uniqueness and updates the owner's group record.
bool GroupRepository::existsName(int userId, const std::string& name) const {
    auto query = database_.prepare(
        "SELECT 1 FROM contact_groups WHERE user_id=? AND name=? LIMIT 1");
    query.bindInt(1, userId).bindText(2, name);
    return query.step();
}

void GroupRepository::rename(int groupId, int userId, const std::string& name) {
    auto query = database_.prepare(
        "UPDATE contact_groups SET name=? WHERE id=? AND user_id=?");
    query.bindText(1, name).bindInt(2, groupId).bindInt(3, userId).execute();
}

bool GroupRepository::remove(int groupId, int userId) {
    if (!findById(groupId, userId).has_value())
        return false;
    auto query = database_.prepare(
        "DELETE FROM contact_groups WHERE id=? AND user_id=?");
    query.bindInt(1, groupId).bindInt(2, userId).execute();
    return true;
}

// Adds and removes contact-to-group links in the join table.
void GroupRepository::addContact(int groupId, int contactId) {
    auto query = database_.prepare(
        "INSERT OR IGNORE INTO contact_group_members(group_id,contact_id) VALUES(?,?)");
    query.bindInt(1, groupId).bindInt(2, contactId).execute();
}

bool GroupRepository::removeContact(int groupId, int contactId) {
    auto query = database_.prepare(
        "DELETE FROM contact_group_members WHERE group_id=? AND contact_id=?");
    query.bindInt(1, groupId).bindInt(2, contactId).execute();
    auto check = database_.prepare(
        "SELECT 1 FROM contact_group_members WHERE group_id=? AND contact_id=?");
    check.bindInt(1, groupId).bindInt(2, contactId);
    return !check.step();
}

int GroupRepository::countAll() const {
    auto query = database_.prepare("SELECT COUNT(*) FROM contact_groups");
    query.step();
    return static_cast<int>(query.columnInt(0));
}
