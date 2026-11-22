#include <QTest>
#include "TestDatabase.h"

class ContactServiceTest : public QObject {
    Q_OBJECT
private slots:
    // Checks that a search predicate can match a contact by name, phone, or email.
    void searchByNamePhoneAndEmail() {
        TestDatabase database;
        QVERIFY(database.isOpen());
        QVERIFY(database.exec(
            "INSERT INTO contacts(user_id, fullname, phone, email) "
            "VALUES(1, 'Nguyen Van An', '0901234567', 'an@example.com')"));
        QCOMPARE(database.scalarInt(
            "SELECT COUNT(*) FROM contacts WHERE user_id = 1 "
            "AND (fullname LIKE '%Van%' OR phone LIKE '%123%' OR email LIKE '%an@%')"),
                 1);
    }
};

QTEST_APPLESS_MAIN(ContactServiceTest)
#include "ContactServiceTest.moc"
