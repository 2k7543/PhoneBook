#include <QTemporaryDir>
#include <QTest>
#include <QFile>
#include <QTextStream>

#include "database/Database.h"
#include "service/PhoneBookRuntime.h"
#include "service/ServiceException.h"

// Integration tests exercise the same services and repositories used by the Qt app.
class PhoneBookRuntimeTest : public QObject {
    Q_OBJECT

private slots:
    // Checks fresh-schema setup, seeded accounts, authentication, and demo groups.
    void initializesFreshDatabaseAndSeedAccounts() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        PhoneBookRuntime runtime(
            directory.filePath("fresh.db").toStdString());

        const RuntimeSession admin =
            runtime.authenticate("admin", "admin123456");
        QVERIFY(admin.admin);
        QCOMPARE(runtime.countUsers(admin.userId), 6);

        const RuntimeSession demo =
            runtime.authenticate("demo", "12345678");
        QVERIFY(!demo.admin);
        QCOMPARE(runtime.listGroups(demo.userId).size(), size_t(2));
    }

    // Proves a stored account remains usable after the runtime is reconstructed.
    void authenticatesAgainAfterRuntimeRestart() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::string path =
            directory.filePath("restart.db").toStdString();
        int firstUserId = 0;
        {
            PhoneBookRuntime runtime(path);
            firstUserId =
                runtime.authenticate("demo", "12345678").userId;
        }

        PhoneBookRuntime runtime(path);
        const RuntimeSession session =
            runtime.authenticate("demo", "12345678");
        QCOMPARE(session.userId, firstUserId);
        QVERIFY(!session.admin);
    }

    // Checks that self-registration persists a regular account that can sign in.
    void registersAccountAndAuthenticates() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        PhoneBookRuntime runtime(
            directory.filePath("registration.db").toStdString());

        const int userId = runtime.registerUser(
            "New User", "new.user", "new.user@example.com", "NewPassword123");
        const RuntimeSession session =
            runtime.authenticate(" NEW.USER ", "NewPassword123");

        QCOMPARE(session.userId, userId);
        QVERIFY(!session.admin);
        QVERIFY_EXCEPTION_THROWN(
            runtime.authenticate("new.user@example.com", "NewPassword123"),
            ServiceException);
        QVERIFY_EXCEPTION_THROWN(
            runtime.registerUser(
                "Another User", "new.user", "other@example.com", "AnotherPass123"),
            ServiceException);
        QVERIFY_EXCEPTION_THROWN(
            runtime.registerUser("Invalid Email", "validuser", "not-an-email", "Password123"),
            ServiceException);
        QVERIFY_EXCEPTION_THROWN(
            runtime.registerUser("Invalid Username", "bad username",
                                 "valid@example.com", "Password123"),
            ServiceException);
    }

    // Covers legacy password migration, admin operations, contacts, CSV, reset, and locking.
    void migratesPasswordsAndAuthenticates() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::string path =
            directory.filePath("runtime.db").toStdString();
        {
            Database database(path);
            database.execute(
                "CREATE TABLE users ("
                "id INTEGER PRIMARY KEY, fullname TEXT, email TEXT UNIQUE, "
                "password TEXT, role TEXT, is_active INTEGER)");
            database.execute(
                "CREATE TABLE AdminLog ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, admin_id INTEGER, "
                "target_user_id INTEGER, action TEXT, detail TEXT, "
                "created_at TEXT)");
            database.execute(
                "CREATE TABLE contacts ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, "
                "lastname TEXT, firstname TEXT, fullname TEXT, phone TEXT, "
                "email TEXT, company TEXT, address TEXT, note TEXT, "
                "favorite INTEGER DEFAULT 0)");
            database.execute(
                "CREATE TABLE contact_groups ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, name TEXT)");
            database.execute(
                "CREATE TABLE contact_group_members (group_id INTEGER, contact_id INTEGER)");
            database.execute(
                "INSERT INTO users VALUES"
                "(1, 'Admin', 'admin@example.com', 'adminpass1', 'admin', 1),"
                "(2, 'User', 'user@example.com', 'userpass1', 'user', 1)");
            database.execute(
                "INSERT INTO users VALUES"
                "(3, 'Another User', 'user@other.example', 'userpass2', 'user', 1)");
        }

        PhoneBookRuntime runtime(path);
        // Opens the legacy schema, then verifies migrated credentials and admin listing.
        runtime.migratePasswords();
        const RuntimeSession session =
            runtime.authenticate("user", "userpass1");
        QCOMPARE(session.userId, 2);
        QVERIFY(!session.admin);
        QCOMPARE(runtime.authenticate("user3", "userpass2").userId, 3);

        auto users = runtime.listUsers(1, "user@example.com");
        QCOMPARE(users.size(), size_t(1));
        QCOMPARE(users.front().id, 2);
        QCOMPARE(QString::fromStdString(users.front().name), QString("User"));

        // Exercises contact CRUD, group membership, and CSV import/export through the runtime.
        RuntimeContact contact;
        contact.lastName = "Nguyen";
        contact.firstName = "An";
        contact.phone = "0900000000";
        const int contactId = runtime.addContact(2, contact);
        contact.firstName = "Binh";
        runtime.updateContact(2, contactId, contact);
        QVERIFY(runtime.toggleFavorite(2, contactId));
        const int groupId = runtime.createGroup(2, "Family");
        runtime.setContactGroup(2, contactId, groupId);
        QCOMPARE(runtime.listContacts(
                     2, "Binh", groupId, true,
                     ContactSort::FirstNameAsc).size(), size_t(1));
        const QString csvPath = directory.filePath("contacts.csv");
        QFile csv(csvPath);
        QVERIFY(csv.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream output(&csv);
        output << "lastname,firstname,fullname,phone,email,company,address,note\n"
               << "Tran,An,Tran An,0911111111,an@example.com,Company,Address,Note\n";
        csv.close();
        const ImportResult imported =
            runtime.importContacts(2, csvPath.toStdString());
        QCOMPARE(imported.imported, 1);
        QCOMPARE(runtime.exportContacts(
                     2, directory.filePath("export.csv").toStdString()),
                 2);
        runtime.removeGroup(2, groupId);
        runtime.removeContact(2, contactId);

        const auto emailResetToken =
            runtime.requestPasswordReset(" USER@example.com ");
        QVERIFY(emailResetToken.has_value());
        runtime.resetPassword(*emailResetToken, "SelfServicePass123");
        QCOMPARE(runtime.authenticate("user", "SelfServicePass123").userId, 2);
        QVERIFY(!runtime.requestPasswordReset("missing@example.com").has_value());

        // Verifies reset-token invalidation, one-time redemption, and locked-account rejection.
        const std::string firstToken = runtime.issuePasswordResetToken(1, 2);
        const std::string token = runtime.issuePasswordResetToken(1, 2);
        QVERIFY_EXCEPTION_THROWN(
            runtime.resetPassword(firstToken, "ChangedPass123"),
            ServiceException);
        runtime.resetPassword(token, "ChangedPass123");
        const RuntimeSession resetSession =
            runtime.authenticate("user", "ChangedPass123");
        QCOMPARE(resetSession.userId, 2);

        runtime.setUserActive(1, 2, false);
        QVERIFY_EXCEPTION_THROWN(
            runtime.authenticate("user", "userpass1"),
            ServiceException);
        QCOMPARE(runtime.countUsers(1), 2);
    }

    // Ensures regular users cannot call administrator-only operations.
    void rejectsAdminOperationFromRegularUser() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::string path =
            directory.filePath("permissions.db").toStdString();
        {
            Database database(path);
            database.execute(
                "CREATE TABLE users ("
                "id INTEGER PRIMARY KEY, fullname TEXT, email TEXT UNIQUE, "
                "password TEXT, role TEXT, is_active INTEGER)");
            database.execute(
                "CREATE TABLE AdminLog ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, admin_id INTEGER, "
                "target_user_id INTEGER, action TEXT, detail TEXT, "
                "created_at TEXT)");
            database.execute(
                "INSERT INTO users VALUES"
                "(1, 'User', 'user@example.com', 'x', 'user', 1)");
        }
        PhoneBookRuntime runtime(path);
        QVERIFY_EXCEPTION_THROWN(runtime.listUsers(1, ""), ServiceException);
        QVERIFY_EXCEPTION_THROWN(
            runtime.issuePasswordResetToken(1, 1), ServiceException);
    }
};

QTEST_APPLESS_MAIN(PhoneBookRuntimeTest)
#include "PhoneBookRuntimeTest.moc"
