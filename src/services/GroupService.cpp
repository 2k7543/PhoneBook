#include "service/GroupService.h"
#include "utils/Validator.h"

using namespace std;

// Validates group names and enforces user ownership for group operations.
GroupService::GroupService(Database& db, GroupRepository& groups, ContactRepository& contacts)
    : db_(db), groups_(groups), contacts_(contacts) {}

string GroupService::validateName(const string& name) {
    string n = Validator::trim(name);
    if (n.empty() || n.size() > 50)
        throw ServiceException(ErrorCode::InvalidInput, "Tên nhóm phải từ 1 đến 50 ký tự");
    return n;
}

// Creates or renames a user's group after checking the name constraints.
int GroupService::create(int userId, const string& name) {
    string n = validateName(name);
    if (groups_.existsName(userId, n))
        throw ServiceException(ErrorCode::AlreadyExists, "Tên nhóm đã tồn tại");
    ContactGroup g;
    g.userId = userId;
    g.name = n;
    return groups_.insert(g);
}

void GroupService::rename(int userId, int groupId, const string& newName) {
    string n = validateName(newName);
    auto g = groups_.findById(groupId, userId);
    if (!g) throw ServiceException(ErrorCode::NotFound, "Không tìm thấy nhóm");
    if (g->name != n && groups_.existsName(userId, n))
        throw ServiceException(ErrorCode::AlreadyExists, "Tên nhóm đã tồn tại");
    groups_.rename(groupId, userId, n);
}

// Removes groups and maintains contact membership links.
void GroupService::remove(int userId, int groupId) {
    Transaction tx(db_);
    if (!groups_.remove(groupId, userId))
        throw ServiceException(ErrorCode::NotFound, "Không tìm thấy nhóm");
    tx.commit();
}

vector<ContactGroup> GroupService::list(int userId) const { return groups_.findByUser(userId); }

void GroupService::addContact(int userId, int groupId, int contactId) {
    if (!groups_.findById(groupId, userId)) throw ServiceException(ErrorCode::NotFound, "Không tìm thấy nhóm");
    if (!contacts_.findById(contactId, userId)) throw ServiceException(ErrorCode::NotFound, "Không tìm thấy liên hệ");
    groups_.addContact(groupId, contactId);
}

void GroupService::removeContact(int userId, int groupId, int contactId) {
    if (!groups_.findById(groupId, userId)) throw ServiceException(ErrorCode::NotFound, "Không tìm thấy nhóm");
    if (!groups_.removeContact(groupId, contactId))
        throw ServiceException(ErrorCode::NotFound, "Liên hệ không thuộc nhóm này");
}
