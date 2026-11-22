#include <QTest>
#include "TestDatabase.h"

class ContactTest : public QObject {
    Q_OBJECT
private slots:
    // Exercises the basic contact insert, update, and delete database operations.
    void insertUpdateAndDeleteContact() {
        TestDatabase database;
        QVERIFY(database.isOpen());
        QVERIFY(database.exec(
            "INSERT INTO contacts(user_id, fullname, phone) "
            "VALUES(1, 'Nguyen Van A', '0901234567')"));
        QVERIFY(database.exec(
            "UPDATE contacts SET fullname = 'Nguyen Van B' "
            "WHERE phone = '0901234567'"));
        QCOMPARE(database.scalarText(
                     "SELECT fullname FROM contacts "
                     "WHERE phone = '0901234567'"),
                 QString("Nguyen Van B"));
        QVERIFY(database.exec("DELETE FROM contacts WHERE phone = '0901234567'"));
        QCOMPARE(database.scalarInt("SELECT COUNT(*) FROM contacts"), 0);
    }
};

QTEST_APPLESS_MAIN(ContactTest)
#include "ContactTest.moc"
