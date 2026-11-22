#include <QTemporaryDir>
#include <QTest>

#include "database/Database.h"
#include "database/DatabaseInitializer.h"
#include "repositories/UserRepository.h"
#include "service/AuthService.h"

class AuthServiceTest : public QObject {
    Q_OBJECT
private slots:
    // Registers and authenticates a user, including case/space normalization of username.
    void loginWithValidCredentials() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Database database(directory.filePath("auth.db").toStdString());
        DatabaseInitializer::initialize(database);
        UserRepository users(database);
        PasswordHasher hasher;
        AuthService auth(users, hasher);

        const User registered =
            auth.registerUser("Demo", "demo", "demo@example.com", "password123");
        const User loggedIn = auth.login(" DEMO ", "password123");

        QCOMPARE(loggedIn.idUser, registered.idUser);
        QVERIFY(auth.isLoggedIn());
        QVERIFY_EXCEPTION_THROWN(
            auth.login("demo@example.com", "password123"), ServiceException);
    }

    void registrationRejectsDuplicateOrInvalidUsername() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Database database(directory.filePath("auth.db").toStdString());
        DatabaseInitializer::initialize(database);
        UserRepository users(database);
        PasswordHasher hasher;
        AuthService auth(users, hasher);
        auth.registerUser("Demo", "demo", "demo@example.com", "password123");

        QVERIFY_EXCEPTION_THROWN(
            auth.registerUser("Other", "DEMO", "other@example.com", "password123"),
            ServiceException);
        QVERIFY_EXCEPTION_THROWN(
            auth.registerUser("Other", "bad username", "other@example.com", "password123"),
            ServiceException);
    }

    // Confirms invalid credentials raise an authentication error without a session.
    void loginWithInvalidCredentialsFails() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Database database(directory.filePath("auth.db").toStdString());
        DatabaseInitializer::initialize(database);
        UserRepository users(database);
        PasswordHasher hasher;
        AuthService auth(users, hasher);
        auth.registerUser("Demo", "demo", "demo@example.com", "password123");

        QVERIFY_EXCEPTION_THROWN(
            auth.login("missing", "wrong"), ServiceException);
        QVERIFY(!auth.isLoggedIn());
    }

    // Verifies that an explicit logout permits another successful login.
    void logoutAllowsLoginAgain() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Database database(directory.filePath("auth.db").toStdString());
        DatabaseInitializer::initialize(database);
        UserRepository users(database);
        PasswordHasher hasher;
        AuthService auth(users, hasher);
        auth.registerUser("Demo", "demo", "demo@example.com", "password123");

        const User firstLogin = auth.login("demo", "password123");
        auth.logout();
        QVERIFY(!auth.isLoggedIn());

        const User secondLogin = auth.login("demo", "password123");
        QCOMPARE(secondLogin.idUser, firstLogin.idUser);
        QVERIFY(auth.isLoggedIn());
    }

    // Ensures a failed second attempt cannot leave a previous session active.
    void failedLoginClearsExistingSession() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Database database(directory.filePath("auth.db").toStdString());
        DatabaseInitializer::initialize(database);
        UserRepository users(database);
        PasswordHasher hasher;
        AuthService auth(users, hasher);
        auth.registerUser("Demo", "demo", "demo@example.com", "password123");
        auth.login("demo", "password123");

        QVERIFY_EXCEPTION_THROWN(
            auth.login("demo", "wrong-password"), ServiceException);
        QVERIFY(!auth.isLoggedIn());

        auth.login("demo", "password123");
        QVERIFY(auth.isLoggedIn());
    }
};

QTEST_APPLESS_MAIN(AuthServiceTest)
#include "AuthServiceTest.moc"
