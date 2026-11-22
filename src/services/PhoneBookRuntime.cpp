#include "service/PhoneBookRuntime.h"

#include "database/DatabaseInitializer.h"
#include "utils/DateTime.h"

#include <algorithm>

// Composes repositories and services, then exposes application operations to Qt.
PhoneBookRuntime::PhoneBookRuntime(const std::string& databasePath)
    : database_(databasePath),
      users_(database_),
      contacts_(database_),
      groups_(database_),
      adminLogs_(database_),
      authService_(users_, passwordHasher_),
      contactService_(database_, contacts_, groups_),
      groupService_(database_, groups_, contacts_),
      adminService_(users_, contacts_, groups_, adminLogs_, passwordHasher_),
      csvService_(contactService_, contacts_),
      passwordResetService_(database_, users_, passwordHasher_) {
    DatabaseInitializer::initialize(database_);
    const bool isNewDatabase = users_.countAll() == 0;
    DatabaseInitializer::seedAdmin(
        database_, "admin@phonebook.local", "Quản trị viên",
        PasswordHasher::hashPassword("admin123456"));
    if (isNewDatabase)
        seedDefaultAccountsAndContacts();
    migratePasswords();
}

// Converts any legacy plaintext passwords to the current PBKDF2 format once.
void PhoneBookRuntime::migratePasswords() {
    for (const User& user : users_.allForPasswordMigration()) {
        if (user.passwordHash.rfind("pbkdf2_sha256$", 0) == 0)
            continue;
        const std::string hash = PasswordHasher::hashPassword(user.passwordHash);
        if (hash.empty())
            throw DatabaseException("Could not securely hash an existing password");
        users_.updatePassword(user.idUser, hash);
    }
}

// Creates sample accounts and demo contacts only for a newly created database.
void PhoneBookRuntime::seedDefaultAccountsAndContacts() {
    const struct {
        const char* name;
        const char* username;
        const char* email;
        const char* password;
    } accounts[] = {
        {"Demo User", "demo", "demo@phonebook.local", "12345678"},
        {"Nguyen Van An", "an", "an@phonebook.local", "12345678"},
        {"Tran Thi Binh", "binh", "binh@phonebook.local", "12345678"},
        {"Le Van Cuong", "cuong", "cuong@phonebook.local", "12345678"},
        {"Pham Thi Dao", "dao", "dao@phonebook.local", "12345678"},
        {"Hoang Minh", "minh", "minh@phonebook.local", "12345678"}
    };

    for (const auto& account : accounts) {
        if (users_.findByEmail(account.email))
            continue;
        User user;
        user.fullname = account.name;
        user.username = account.username;
        user.email = account.email;
        user.passwordHash = PasswordHasher::hashPassword(account.password);
        user.role = UserRole::User;
        user.isActive = true;
        user.language = "vi";
        user.createDate = DateTime::now();
        users_.insert(user);
    }

    const auto demo = users_.findByEmail("demo@phonebook.local");
    if (!demo)
        return;

    struct SampleContact {
        const char* lastName;
        const char* firstName;
        const char* phone;
        const char* email;
        const char* group;
    };
    const SampleContact samples[] = {
        {"Nguyễn", "Văn A", "0901234567", "nguyenvana@gmail.com", "Gia đình"},
        {"Trần", "Thị B", "0912345678", "tranthib@gmail.com", "Đồng nghiệp"},
        {"Lê", "Văn C", "0987654321", "levanc@gmail.com", "Gia đình"}
    };

    auto demoGroups = groupService_.list(demo->idUser);
    for (const SampleContact& sample : samples) {
        auto group = std::find_if(
            demoGroups.begin(), demoGroups.end(),
            [&sample](const ContactGroup& item) { return item.name == sample.group; });
        int groupId = group == demoGroups.end()
            ? groupService_.create(demo->idUser, sample.group)
            : group->groupId;
        auto duplicate = contacts_.findDuplicate(demo->idUser, {sample.phone});
        int contactId = 0;
        if (duplicate) {
            contactId = duplicate->idContact;
        } else {
            Contact contact;
            contact.lastname = sample.lastName;
            contact.firstname = sample.firstName;
            contact.fullname = std::string(sample.lastName) + " " + sample.firstName;
            contact.company = "Example Company";
            contact.address = "TP.HCM";
            contact.note = sample.group;
            contact.phones.push_back({0, sample.phone, PhoneLabel::Mobile});
            contact.emails.push_back({0, sample.email, EmailLabel::Personal});
            contactId = contactService_.add(demo->idUser, contact);
        }

        const Contact contact = contactService_.view(demo->idUser, contactId);
        if (std::find(contact.groupIds.begin(), contact.groupIds.end(), groupId) ==
            contact.groupIds.end()) {
            groupService_.addContact(demo->idUser, groupId, contactId);
        }
        demoGroups = groupService_.list(demo->idUser);
    }
}

// Authenticates users and returns only the session details needed by the UI.
int PhoneBookRuntime::registerUser(const std::string& fullname,
                                   const std::string& username,
                                   const std::string& email,
                                   const std::string& password) {
    return authService_.registerUser(fullname, username, email, password).idUser;
}

RuntimeSession PhoneBookRuntime::authenticate(const std::string& username,
                                             const std::string& password) {
    const User user = authService_.login(username, password);
    return {user.idUser, user.fullname, user.isAdmin()};
}

// Converts administrator account records into UI-safe summaries.
std::vector<RuntimeUser> PhoneBookRuntime::listUsers(
    int adminId, const std::string& keyword) {
    const auto users = adminService_.listUsers(adminId, keyword, 1);
    std::vector<RuntimeUser> result;
    result.reserve(users.size());
    for (const User& user : users)
        result.push_back({user.idUser, user.fullname, user.isActive});
    return result;
}

// Delegates account counts and lock/unlock actions to AdminService.
int PhoneBookRuntime::countUsers(int adminId) {
    return adminService_.countUsers(adminId);
}

void PhoneBookRuntime::setUserActive(int adminId, int userId, bool active) {
    Transaction transaction(database_);
    if (active)
        adminService_.unlockUser(adminId, userId);
    else
        adminService_.lockUser(adminId, userId);
    transaction.commit();
}

// Maps Qt form values to the canonical contact model.
Contact PhoneBookRuntime::toContact(int userId, const RuntimeContact& input) {
    Contact contact;
    contact.userId = userId;
    contact.lastname = input.lastName;
    contact.firstname = input.firstName;
    contact.fullname = input.lastName + " " + input.firstName;
    contact.company = input.company;
    contact.address = input.address;
    contact.note = input.note;
    if (!input.phone.empty())
        contact.phones.push_back({0, input.phone, PhoneLabel::Mobile});
    if (!input.email.empty())
        contact.emails.push_back({0, input.email, EmailLabel::Personal});
    return contact;
}

// Delegates contact CRUD, favorites, and listing to ContactService.
int PhoneBookRuntime::addContact(int userId, const RuntimeContact& input) {
    return contactService_.add(userId, toContact(userId, input));
}

void PhoneBookRuntime::updateContact(int userId, int contactId,
                                     const RuntimeContact& input) {
    Contact contact = toContact(userId, input);
    contact.idContact = contactId;
    contactService_.update(userId, std::move(contact));
}

void PhoneBookRuntime::removeContact(int userId, int contactId) {
    contactService_.remove(userId, contactId);
}

bool PhoneBookRuntime::toggleFavorite(int userId, int contactId) {
    return contactService_.toggleFavorite(userId, contactId);
}

std::vector<Contact> PhoneBookRuntime::listContacts(
    int userId, const std::string& keyword, int groupId,
    bool favoritesOnly, ContactSort sort) const {
    std::vector<Contact> result;
    for (int page = 1;; ++page) {
        auto batch = contactService_.search(
            userId, keyword, page, groupId, favoritesOnly, sort);
        if (batch.empty())
            break;
        result.insert(result.end(), batch.begin(), batch.end());
    }
    return result;
}

// Delegates group creation, removal, listing, and contact assignment to GroupService.
int PhoneBookRuntime::createGroup(int userId, const std::string& name) {
    return groupService_.create(userId, name);
}

void PhoneBookRuntime::removeGroup(int userId, int groupId) {
    groupService_.remove(userId, groupId);
}

void PhoneBookRuntime::setContactGroup(int userId, int contactId, int groupId) {
    const Contact contact = contactService_.view(userId, contactId);
    const auto availableGroups = groupService_.list(userId);
    if (groupId > 0 &&
        std::none_of(availableGroups.begin(), availableGroups.end(),
                     [groupId](const ContactGroup& group) {
                         return group.groupId == groupId;
                     })) {
        throw ServiceException(ErrorCode::NotFound, "Không tìm thấy nhóm");
    }

    Transaction transaction(database_);
    for (int existingGroupId : contact.groupIds)
        groupService_.removeContact(userId, existingGroupId, contactId);
    if (groupId > 0)
        groupService_.addContact(userId, groupId, contactId);
    transaction.commit();
}

std::vector<RuntimeGroup> PhoneBookRuntime::listGroups(int userId) const {
    const auto groups = groupService_.list(userId);
    std::vector<RuntimeGroup> result;
    result.reserve(groups.size());
    for (const ContactGroup& group : groups)
        result.push_back({group.groupId, group.name});
    return result;
}

std::vector<int> PhoneBookRuntime::contactGroupIds(int userId,
                                                   int contactId) const {
    return contactService_.view(userId, contactId).groupIds;
}

// Delegates CSV and password-reset operations to their domain services.
ImportResult PhoneBookRuntime::importContacts(
    int userId, const std::string& filePath) {
    return csvService_.importContacts(userId, filePath);
}

int PhoneBookRuntime::exportContacts(
    int userId, const std::string& filePath) const {
    return csvService_.exportContacts(userId, filePath);
}

void PhoneBookRuntime::resetPassword(const std::string& token,
                                     const std::string& newPassword) {
    passwordResetService_.resetPassword(token, newPassword);
}

std::string PhoneBookRuntime::issuePasswordResetToken(int adminId, int userId) {
    Transaction transaction(database_);
    const std::string token = adminService_.issuePasswordResetToken(
        adminId, userId, passwordResetService_);
    transaction.commit();
    return token;
}
