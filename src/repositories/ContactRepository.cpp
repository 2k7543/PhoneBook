#include "repositories/ContactRepository.h"

// Maps flat SQLite contact columns into the richer contact model.
namespace {
constexpr const char* kContactColumns =
    "id,user_id,fullname,phone,email,company,address,note,favorite,"
    "lastname,firstname,birthday,avatar,create_date,update_date";

Contact readContact(Statement& query) {
    Contact contact;
    contact.idContact = static_cast<int>(query.columnInt(0));
    contact.userId = static_cast<int>(query.columnInt(1));
    contact.fullname = query.columnText(2);
    contact.lastname = query.columnText(9);
    contact.firstname = query.columnText(10);
    contact.company = query.columnText(5);
    contact.address = query.columnText(6);
    contact.note = query.columnText(7);
    contact.isFavorite = query.columnBool(8);
    contact.birthday = query.columnText(11);
    contact.avatar = query.columnText(12);
    contact.createDate = query.columnText(13);
    contact.updateDate = query.columnText(14);

    const std::string phone = query.columnText(3);
    if (!phone.empty())
        contact.phones.push_back(PhoneNumber{0, phone, PhoneLabel::Mobile});
    const std::string email = query.columnText(4);
    if (!email.empty())
        contact.emails.push_back(ContactEmail{0, email, EmailLabel::Personal});
    return contact;
}

void loadGroupIds(Database& database, Contact& contact) {
    auto query = database.prepare(
        "SELECT group_id FROM contact_group_members WHERE contact_id=? ORDER BY group_id");
    query.bindInt(1, contact.idContact);
    while (query.step())
        contact.groupIds.push_back(static_cast<int>(query.columnInt(0)));
}

std::string sortExpression(ContactSort sort) {
    return sort == ContactSort::FirstNameAsc
        ? "firstname COLLATE NOCASE, lastname COLLATE NOCASE, id"
        : "lastname COLLATE NOCASE, firstname COLLATE NOCASE, id";
}
}

// Reads contacts with their group memberships, always scoped to the owning user.
std::optional<Contact> ContactRepository::findById(int contactId, int userId) const {
    auto query = database_.prepare(
        std::string("SELECT ") + kContactColumns +
        " FROM contacts WHERE id=? AND user_id=?");
    query.bindInt(1, contactId).bindInt(2, userId);
    if (!query.step())
        return std::nullopt;
    Contact contact = readContact(query);
    loadGroupIds(database_, contact);
    return contact;
}

// Lists or searches the owner's contacts with optional group/favorite filters.
std::vector<Contact> ContactRepository::list(
    int userId, int groupId, ContactSort sort, int limit, int offset,
    bool favoritesOnly) const {
    std::string sql = std::string("SELECT ") + kContactColumns +
        " FROM contacts WHERE user_id=?";
    if (groupId > 0)
        sql += " AND EXISTS (SELECT 1 FROM contact_group_members gm "
               "WHERE gm.contact_id=contacts.id AND gm.group_id=?)";
    if (favoritesOnly)
        sql += " AND favorite=1";
    sql += " ORDER BY " + sortExpression(sort) + " LIMIT ? OFFSET ?";

    auto query = database_.prepare(sql);
    query.bindInt(1, userId);
    int parameter = 2;
    if (groupId > 0)
        query.bindInt(parameter++, groupId);
    query.bindInt(parameter++, limit).bindInt(parameter, offset);

    std::vector<Contact> contacts;
    while (query.step()) {
        Contact contact = readContact(query);
        loadGroupIds(database_, contact);
        contacts.push_back(std::move(contact));
    }
    return contacts;
}

std::vector<Contact> ContactRepository::search(
    int userId, const std::string& keyword, int limit, int offset,
    int groupId, bool favoritesOnly, ContactSort sort) const {
    std::string sql = std::string("SELECT ") + kContactColumns +
        " FROM contacts WHERE user_id=? AND "
        "(fullname LIKE ? OR lastname LIKE ? OR firstname LIKE ? OR phone LIKE ? "
        "OR email LIKE ? OR company LIKE ?)";
    if (groupId > 0)
        sql += " AND EXISTS (SELECT 1 FROM contact_group_members gm "
               "WHERE gm.contact_id=contacts.id AND gm.group_id=?)";
    if (favoritesOnly)
        sql += " AND favorite=1";
    sql += " ORDER BY " + sortExpression(sort) + " LIMIT ? OFFSET ?";
    auto query = database_.prepare(sql);
    const std::string pattern = "%" + keyword + "%";
    query.bindInt(1, userId)
        .bindText(2, pattern).bindText(3, pattern).bindText(4, pattern)
        .bindText(5, pattern).bindText(6, pattern).bindText(7, pattern);
    int parameter = 8;
    if (groupId > 0)
        query.bindInt(parameter++, groupId);
    query.bindInt(parameter++, limit).bindInt(parameter, offset);

    std::vector<Contact> contacts;
    while (query.step()) {
        Contact contact = readContact(query);
        loadGroupIds(database_, contact);
        contacts.push_back(std::move(contact));
    }
    return contacts;
}

// Persists a contact's current scalar fields and primary phone/email values.
int ContactRepository::insert(const Contact& contact) {
    const std::string phone = contact.phones.empty()
        ? std::string() : contact.phones.front().number;
    const std::string email = contact.emails.empty()
        ? std::string() : contact.emails.front().email;
    auto query = database_.prepare(
        "INSERT INTO contacts(user_id,fullname,phone,email,company,address,note,"
        "favorite,lastname,firstname,birthday,avatar,create_date,update_date) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    query.bindInt(1, contact.userId)
        .bindText(2, contact.fullname)
        .bindText(3, phone)
        .bindText(4, email)
        .bindText(5, contact.company)
        .bindText(6, contact.address)
        .bindText(7, contact.note)
        .bindInt(8, contact.isFavorite ? 1 : 0)
        .bindText(9, contact.lastname)
        .bindText(10, contact.firstname)
        .bindText(11, contact.birthday)
        .bindText(12, contact.avatar)
        .bindText(13, contact.createDate)
        .bindText(14, contact.updateDate)
        .execute();
    return static_cast<int>(database_.lastInsertId());
}

void ContactRepository::update(const Contact& contact) {
    const std::string phone = contact.phones.empty()
        ? std::string() : contact.phones.front().number;
    const std::string email = contact.emails.empty()
        ? std::string() : contact.emails.front().email;
    auto query = database_.prepare(
        "UPDATE contacts SET fullname=?,phone=?,email=?,company=?,address=?,note=?,"
        "favorite=?,lastname=?,firstname=?,birthday=?,avatar=?,update_date=? "
        "WHERE id=? AND user_id=?");
    query.bindText(1, contact.fullname)
        .bindText(2, phone)
        .bindText(3, email)
        .bindText(4, contact.company)
        .bindText(5, contact.address)
        .bindText(6, contact.note)
        .bindInt(7, contact.isFavorite ? 1 : 0)
        .bindText(8, contact.lastname)
        .bindText(9, contact.firstname)
        .bindText(10, contact.birthday)
        .bindText(11, contact.avatar)
        .bindText(12, contact.updateDate)
        .bindInt(13, contact.idContact)
        .bindInt(14, contact.userId)
        .execute();
}

// Deletes or updates favorite state only for the specified owner's contact.
bool ContactRepository::remove(int contactId, int userId) {
    auto exists = database_.prepare(
        "SELECT 1 FROM contacts WHERE id=? AND user_id=?");
    exists.bindInt(1, contactId).bindInt(2, userId);
    if (!exists.step())
        return false;
    auto query = database_.prepare(
        "DELETE FROM contacts WHERE id=? AND user_id=?");
    query.bindInt(1, contactId).bindInt(2, userId).execute();
    return true;
}

void ContactRepository::setFavorite(int contactId, int userId, bool favorite) {
    auto query = database_.prepare(
        "UPDATE contacts SET favorite=? WHERE id=? AND user_id=?");
    query.bindInt(1, favorite ? 1 : 0)
        .bindInt(2, contactId).bindInt(3, userId).execute();
}

// Detects phone-number duplicates within one user's address book.
std::optional<Contact> ContactRepository::findDuplicate(
    int userId, const std::vector<std::string>& numbers,
    int exceptContactId) const {
    for (const std::string& number : numbers) {
        auto query = database_.prepare(
            "SELECT id FROM contacts WHERE user_id=? AND phone=? AND id<>? LIMIT 1");
        query.bindInt(1, userId).bindText(2, number).bindInt(3, exceptContactId);
        if (query.step())
            return findById(static_cast<int>(query.columnInt(0)), userId);
    }
    return std::nullopt;
}

// Returns the total number of stored contacts.
int ContactRepository::countAll() const {
    auto query = database_.prepare("SELECT COUNT(*) FROM contacts");
    query.step();
    return static_cast<int>(query.columnInt(0));
}
