#pragma once
#include <string>
#include <vector>
#include "database/Database.h"
#include "models/ContactGroup.h"
#include "repositories/ContactRepository.h"
#include "repositories/GroupRepository.h"
#include "service/ServiceException.h"
using namespace std;

class GroupService {
public:
    // Refactor note: group ownership, name validation, and membership updates
    // are handled consistently for all Qt group controls.
    GroupService(Database& db, GroupRepository& groups, ContactRepository& contacts);

    int create(int userId, const string& name);
    void rename(int userId, int groupId, const string& newName);
    void remove(int userId, int groupId);
    vector<ContactGroup> list(int userId) const;
    void addContact(int userId, int groupId, int contactId);
    void removeContact(int userId, int groupId, int contactId);

private:
    Database& db_;
    GroupRepository& groups_;
    ContactRepository& contacts_;

    static string validateName(const string& name);
};
