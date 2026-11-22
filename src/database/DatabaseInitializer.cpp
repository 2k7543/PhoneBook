#include "database/DatabaseInitializer.h"

#include "utils/DateTime.h"
#include "utils/Validator.h"

#include <cctype>
#include <unordered_set>
#include <vector>

// Provides the schema shared by the Qt model and C++ repositories, including upgrades.
namespace {
// Schema inspection lets startup apply additive migrations only when needed.
bool hasColumn(Database& database, const std::string& table,
               const std::string& column) {
    auto query = database.prepare("PRAGMA table_info(" + table + ")");
    while (query.step()) {
        if (query.columnText(1) == column)
            return true;
    }
    return false;
}

void ensureColumn(Database& database, const std::string& table,
                  const std::string& column, const std::string& definition) {
    if (!hasColumn(database, table, column))
        database.execute("ALTER TABLE " + table + " ADD COLUMN " +
                         column + " " + definition);
}

void backfillUsernames(Database& database) {
    struct ExistingUser {
        int id;
        std::string email;
        std::string username;
    };

    std::vector<ExistingUser> existingUsers;
    auto query = database.prepare("SELECT id,email,username FROM users ORDER BY id");
    while (query.step()) {
        existingUsers.push_back({
            static_cast<int>(query.columnInt(0)),
            query.columnText(1),
            query.columnText(2)
        });
    }

    std::unordered_set<std::string> usedUsernames;
    for (const ExistingUser& user : existingUsers) {
        std::string username = Validator::toLower(Validator::trim(user.username));
        if (username.empty()) {
            const size_t separator = user.email.find('@');
            username = separator == std::string::npos
                ? std::string()
                : user.email.substr(0, separator);
        }

        std::string sanitized;
        for (unsigned char character : username) {
            if (std::isalnum(character) != 0 ||
                character == '.' || character == '_' || character == '-') {
                sanitized.push_back(static_cast<char>(character));
            } else {
                sanitized.push_back('_');
            }
        }
        username = Validator::toLower(sanitized);
        if (!Validator::isValidUsername(username))
            username = "user" + std::to_string(user.id);

        if (usedUsernames.count(username) != 0) {
            username = "user" + std::to_string(user.id);
            int suffix = 0;
            while (usedUsernames.count(username) != 0) {
                username = "user" + std::to_string(user.id) +
                    "_" + std::to_string(++suffix);
            }
        }

        usedUsernames.insert(username);
        if (username != user.username) {
            auto update = database.prepare(
                "UPDATE users SET username=? WHERE id=?");
            update.bindText(1, username).bindInt(2, user.id).execute();
        }
    }
}
}

// Creates the application schema and upgrades existing databases in one transaction.
void DatabaseInitializer::initialize(Database& database) {
    Transaction transaction(database);
    database.execute(
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "fullname TEXT NOT NULL,"
        "username TEXT NOT NULL DEFAULT '',"
        "email TEXT NOT NULL UNIQUE,"
        "password TEXT NOT NULL,"
        "role TEXT NOT NULL DEFAULT 'user',"
        "is_active INTEGER NOT NULL DEFAULT 1,"
        "language TEXT NOT NULL DEFAULT 'vi',"
        "create_date TEXT NOT NULL DEFAULT '')");
    database.execute(
        "CREATE TABLE IF NOT EXISTS contacts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "fullname TEXT NOT NULL,"
        "phone TEXT NOT NULL DEFAULT '',"
        "email TEXT NOT NULL DEFAULT '',"
        "company TEXT NOT NULL DEFAULT '',"
        "address TEXT NOT NULL DEFAULT '',"
        "note TEXT NOT NULL DEFAULT '',"
        "favorite INTEGER NOT NULL DEFAULT 0,"
        "lastname TEXT NOT NULL DEFAULT '',"
        "firstname TEXT NOT NULL DEFAULT '',"
        "birthday TEXT NOT NULL DEFAULT '',"
        "avatar TEXT NOT NULL DEFAULT '',"
        "create_date TEXT NOT NULL DEFAULT '',"
        "update_date TEXT NOT NULL DEFAULT '')");
    database.execute(
        "CREATE TABLE IF NOT EXISTS contact_groups ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "name TEXT NOT NULL, UNIQUE(user_id,name))");
    database.execute(
        "CREATE TABLE IF NOT EXISTS contact_group_members ("
        "group_id INTEGER NOT NULL REFERENCES contact_groups(id) ON DELETE CASCADE,"
        "contact_id INTEGER NOT NULL REFERENCES contacts(id) ON DELETE CASCADE,"
        "PRIMARY KEY(group_id,contact_id))");
    database.execute(
        "CREATE TABLE IF NOT EXISTS AdminLog ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "admin_id INTEGER NOT NULL,"
        "target_user_id INTEGER,"
        "action TEXT NOT NULL,"
        "detail TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)");
    database.execute(
        "CREATE TABLE IF NOT EXISTS password_reset_tokens ("
        "token TEXT PRIMARY KEY,"
        "user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "expires_at TEXT NOT NULL,"
        "used INTEGER NOT NULL DEFAULT 0)");

    ensureColumn(database, "users", "role", "TEXT NOT NULL DEFAULT 'user'");
    ensureColumn(database, "users", "username", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(database, "users", "is_active", "INTEGER NOT NULL DEFAULT 1");
    ensureColumn(database, "users", "language", "TEXT NOT NULL DEFAULT 'vi'");
    ensureColumn(database, "users", "create_date", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(database, "contacts", "lastname", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(database, "contacts", "firstname", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(database, "contacts", "birthday", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(database, "contacts", "avatar", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(database, "contacts", "create_date", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(database, "contacts", "update_date", "TEXT NOT NULL DEFAULT ''");

    backfillUsernames(database);
    database.execute(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_users_username "
        "ON users(lower(username))");

    // Backfill split names, remove duplicate phone records, and index common lookups.
    database.execute(
        "UPDATE contacts SET "
        "lastname=CASE WHEN instr(fullname,' ')>0 "
        "THEN substr(fullname,1,instr(fullname,' ')-1) ELSE fullname END,"
        "firstname=CASE WHEN instr(fullname,' ')>0 "
        "THEN substr(fullname,instr(fullname,' ')+1) ELSE '' END "
        "WHERE trim(lastname)='' AND trim(firstname)=''");
    database.execute(
        "DELETE FROM contacts WHERE id NOT IN "
        "(SELECT MIN(id) FROM contacts GROUP BY user_id,phone)");
    database.execute(
        "CREATE INDEX IF NOT EXISTS idx_contacts_user_name "
        "ON contacts(user_id,lastname,firstname)");
    database.execute(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_contacts_user_phone "
        "ON contacts(user_id,phone)");
    database.execute(
        "CREATE INDEX IF NOT EXISTS idx_groups_user ON contact_groups(user_id)");
    transaction.commit();
}

// Seeds the default administrator only when no administrator account exists.
bool DatabaseInitializer::seedAdmin(Database& database, const std::string& email,
                                    const std::string& fullname,
                                    const std::string& passwordHash) {
    auto check = database.prepare("SELECT COUNT(*) FROM users WHERE role='admin'");
    check.step();
    if (check.columnInt(0) > 0)
        return false;

    const size_t separator = email.find('@');
    const std::string username = separator == std::string::npos
        ? email
        : email.substr(0, separator);
    auto insert = database.prepare(
        "INSERT INTO users(fullname,username,email,password,role,is_active,language,create_date) "
        "VALUES(?,?,?,?,'admin',1,'vi',?)");
    insert.bindText(1, fullname)
        .bindText(2, username)
        .bindText(3, email)
        .bindText(4, passwordHash)
        .bindText(5, DateTime::now())
        .execute();
    return true;
}
