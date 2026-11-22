#pragma once

#include <optional>
#include <string>
#include <vector>

#include "database/Database.h"
#include "models/Contact.h"

// Refactor note: contact persistence is centralized here instead of being
// duplicated between widgets, services, and in-memory repository stubs.
class ContactRepository {
public:
    explicit ContactRepository(Database& database) : database_(database) {}

    std::optional<Contact> findById(int contactId, int userId) const;
    std::vector<Contact> list(int userId, int groupId, ContactSort sort,
                              int limit, int offset,
                              bool favoritesOnly = false) const;
    std::vector<Contact> search(int userId, const std::string& keyword,
                                int limit, int offset, int groupId = 0,
                                bool favoritesOnly = false,
                                ContactSort sort = ContactSort::LastNameAsc) const;
    int insert(const Contact& contact);
    void update(const Contact& contact);
    bool remove(int contactId, int userId);
    void setFavorite(int contactId, int userId, bool favorite);
    std::optional<Contact> findDuplicate(int userId,
                                         const std::vector<std::string>& numbers,
                                         int exceptContactId = 0) const;
    int countAll() const;

private:
    Database& database_;
};
