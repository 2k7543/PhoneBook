#include "service/ContactService.h"
#include "utils/DateTime.h"
#include "utils/Validator.h"
using namespace std;

// Applies common contact normalization, ownership checks, validation, and persistence.
ContactService::ContactService(Database& db, ContactRepository& contacts, GroupRepository& groups)
    : db_(db), contacts_(contacts), groups_(groups) {}

void ContactService::normalize(Contact& c) {
    c.lastname = Validator::trim(c.lastname);
    c.firstname = Validator::trim(c.firstname);
    c.fullname = c.lastname.empty() && c.firstname.empty()
        ? Validator::trim(c.fullname)
        : Validator::trim(c.lastname + " " + c.firstname);
    c.company = Validator::trim(c.company);
    c.address = Validator::trim(c.address);
    c.note = Validator::trim(c.note);
    c.birthday = Validator::trim(c.birthday);
    for (auto& p : c.phones) p.number = p.normalize();
    for (auto& e : c.emails) e.email = Validator::toLower(Validator::trim(e.email));
}

// Lists and searches contacts with paging and optional group/favorite filters.
vector<Contact> ContactService::list(int userId, int page, ContactSort sort,
                                     int groupId, bool favoritesOnly) const {
    if (page < 1) page = 1;
    if (groupId > 0 && !groups_.findById(groupId, userId))
        throw ServiceException(ErrorCode::NotFound, "Không tìm thấy nhóm");
    return contacts_.list(userId, groupId, sort, kPageSize,
                          (page - 1) * kPageSize, favoritesOnly);
}

vector<Contact> ContactService::search(int userId, const string& keyword,
                                       int page, int groupId,
                                       bool favoritesOnly, ContactSort sort) const {
    if (page < 1) page = 1;
    string kw = Validator::trim(keyword);
    if (kw.empty())
        return list(userId, page, sort, groupId, favoritesOnly);
    if (groupId > 0 && !groups_.findById(groupId, userId))
        throw ServiceException(ErrorCode::NotFound, "Không tìm thấy nhóm");
    return contacts_.search(userId, kw, kPageSize, (page - 1) * kPageSize,
                            groupId, favoritesOnly, sort);
}

// Loads one owned contact and checks phone-number duplicates before writes.
Contact ContactService::view(int userId, int contactId) const {
    auto c = contacts_.findById(contactId, userId);
    if (!c) throw ServiceException(ErrorCode::NotFound, "Không tìm thấy liên hệ");
    return *c;
}

optional<Contact> ContactService::findDuplicate(int userId, const Contact& contact) const {
    vector<string> numbers;
    for (const auto& p : contact.phones) numbers.push_back(p.normalize());
    return contacts_.findDuplicate(userId, numbers, contact.idContact);
}

// Validates and persists contact creation or updates in a database transaction.
int ContactService::add(int userId, Contact contact, bool force) {
    normalize(contact);
    string err;
    if (!contact.validate(&err)) throw ServiceException(ErrorCode::InvalidInput, err);

    contact.userId = userId;
    contact.idContact = 0;
    if (!force) {
        if (auto dup = findDuplicate(userId, contact))
            throw ServiceException(ErrorCode::Duplicate, "Trùng số điện thoại với liên hệ: " + dup->fullname);
    }

    contact.createDate = contact.updateDate = DateTime::now();
    Transaction tx(db_);
    int id = contacts_.insert(contact);
    tx.commit();
    return id;
}

void ContactService::update(int userId, Contact contact, bool force) {
    auto existing = contacts_.findById(contact.idContact, userId);
    if (!existing) throw ServiceException(ErrorCode::NotFound, "Không tìm thấy liên hệ");

    normalize(contact);
    string err;
    if (!contact.validate(&err)) throw ServiceException(ErrorCode::InvalidInput, err);

    contact.userId = userId;
    if (!force) {
        if (auto dup = findDuplicate(userId, contact))
            throw ServiceException(ErrorCode::Duplicate, "Trùng số điện thoại với liên hệ: " + dup->fullname);
    }

    contact.createDate = existing->createDate;
    contact.updateDate = DateTime::now();
    Transaction tx(db_);
    contacts_.update(contact);
    tx.commit();
}

// Deletes an owned contact or toggles its favorite flag.
void ContactService::remove(int userId, int contactId) {
    Transaction tx(db_);
    if (!contacts_.remove(contactId, userId))
        throw ServiceException(ErrorCode::NotFound, "Không tìm thấy liên hệ");
    tx.commit();
}

bool ContactService::toggleFavorite(int userId, int contactId) {
    auto c = contacts_.findById(contactId, userId);
    if (!c) throw ServiceException(ErrorCode::NotFound, "Không tìm thấy liên hệ");
    bool newState = !c->isFavorite;
    contacts_.setFavorite(contactId, userId, newState);
    return newState;
}
