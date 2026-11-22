#pragma once
#include <optional>
#include <string>
#include <vector>
#include "database/Database.h"
#include "models/Contact.h"
#include "repositories/ContactRepository.h"
#include "repositories/GroupRepository.h"
#include "service/ServiceException.h"
using namespace std;

class ContactService {
public:
    // Refactor note: contact validation and ownership rules are centralized
    // here before repository writes.
    static constexpr int kPageSize = 20;

    ContactService(Database& db, ContactRepository& contacts, GroupRepository& groups);

    vector<Contact> list(int userId, int page,
                         ContactSort sort = ContactSort::NameAsc,
                         int groupId = 0, bool favoritesOnly = false) const;
    vector<Contact> search(int userId, const string& keyword, int page,
                           int groupId = 0, bool favoritesOnly = false,
                           ContactSort sort = ContactSort::NameAsc) const;
    Contact view(int userId, int contactId) const;

    int add(int userId, Contact contact, bool force = false);
    void update(int userId, Contact contact, bool force = false);
    void remove(int userId, int contactId);
    bool toggleFavorite(int userId, int contactId);
    optional<Contact> findDuplicate(int userId, const Contact& contact) const;

private:
    Database& db_;
    ContactRepository& contacts_;
    GroupRepository& groups_;

    static void normalize(Contact& c);
};
