#include <QTest>
#include "TestDatabase.h"

class UserTest : public QObject {
    Q_OBJECT
private slots:
    // Checks that an inserted account can be read back from SQLite.
    void insertAndFindUser() {
        TestDatabase database;
        QVERIFY(database.isOpen());
        QVERIFY(database.exec(
            "INSERT INTO users(fullname, email, password) "
            "VALUES('Test User', 'test@example.com', 'password123')"));
        QCOMPARE(database.scalarText(
                     "SELECT fullname FROM users WHERE email = 'test@example.com'"),
                 QString("Test User"));
    }

    // Ensures the database's unique email constraint rejects duplicate accounts.
    void duplicateEmailIsRejected() {
        TestDatabase database;
        QVERIFY(database.isOpen());
        QVERIFY(database.exec(
            "INSERT INTO users(fullname, email, password) "
            "VALUES('First', 'same@example.com', 'password123')"));
        QVERIFY(!database.exec(
            "INSERT INTO users(fullname, email, password) "
            "VALUES('Second', 'same@example.com', 'password123')"));
    }
};

QTEST_APPLESS_MAIN(UserTest)
#include "UserTest.moc"
