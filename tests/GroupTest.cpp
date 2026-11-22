#include <QTest>
#include "TestDatabase.h"

class GroupTest : public QObject {
    Q_OBJECT
private slots:
    // Verifies that a user cannot create two groups with the same name.
    void createUniqueGroup() {
        TestDatabase database;
        QVERIFY(database.isOpen());
        QVERIFY(database.exec(
            "INSERT INTO contact_groups(user_id, name) VALUES(1, 'Family')"));
        QVERIFY(!database.exec(
            "INSERT INTO contact_groups(user_id, name) VALUES(1, 'Family')"));
    }

    // Confirms that group counts are scoped to the owning user.
    void listGroupsForUser() {
        TestDatabase database;
        QVERIFY(database.exec(
            "INSERT INTO contact_groups(user_id, name) VALUES(1, 'Family')"));
        QVERIFY(database.exec(
            "INSERT INTO contact_groups(user_id, name) VALUES(1, 'Work')"));
        QVERIFY(database.exec(
            "INSERT INTO contact_groups(user_id, name) VALUES(2, 'Friends')"));
        QCOMPARE(database.scalarInt(
                     "SELECT COUNT(*) FROM contact_groups WHERE user_id = 1"),
                 2);
    }
};

QTEST_APPLESS_MAIN(GroupTest)
#include "GroupTest.moc"
