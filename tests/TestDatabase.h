#pragma once

#include <QTemporaryDir>
#include <QString>
#include <sqlite3.h>

class TestDatabase {
public:
    TestDatabase() {
        if (!temporary_.isValid())
            return;
        sqlite3_open(temporary_.filePath("test.db").toUtf8().constData(), &database_);
        createSchema();
    }

    ~TestDatabase() {
        if (database_)
            sqlite3_close(database_);
    }

    bool isOpen() const { return database_ != nullptr; }
    sqlite3* database() { return database_; }

    bool exec(const QString& sql) const {
        char* error = nullptr;
        int result = sqlite3_exec(database_, sql.toUtf8().constData(), nullptr, nullptr, &error);
        if (error)
            sqlite3_free(error);
        return result == SQLITE_OK;
    }

    int scalarInt(const QString& sql) const {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(database_, sql.toUtf8().constData(), -1, &statement, nullptr) != SQLITE_OK)
            return -1;
        int value = sqlite3_step(statement) == SQLITE_ROW ? sqlite3_column_int(statement, 0) : -1;
        sqlite3_finalize(statement);
        return value;
    }

    QString scalarText(const QString& sql) const {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(database_, sql.toUtf8().constData(), -1, &statement, nullptr) != SQLITE_OK)
            return {};
        QString value;
        if (sqlite3_step(statement) == SQLITE_ROW) {
            const unsigned char* text = sqlite3_column_text(statement, 0);
            if (text)
                value = QString::fromUtf8(reinterpret_cast<const char*>(text));
        }
        sqlite3_finalize(statement);
        return value;
    }

private:
    QTemporaryDir temporary_;
    sqlite3* database_ = nullptr;

    void createSchema() {
        exec(
            "CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "fullname TEXT NOT NULL, email TEXT NOT NULL UNIQUE, password TEXT NOT NULL)");
        exec(
            "CREATE TABLE contact_groups (id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "user_id INTEGER NOT NULL, name TEXT NOT NULL,"
            "UNIQUE(user_id, name))");
        exec(
            "CREATE TABLE contacts (id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "user_id INTEGER NOT NULL, fullname TEXT NOT NULL, phone TEXT NOT NULL,"
            "email TEXT, company TEXT, address TEXT, note TEXT,"
            "favorite INTEGER NOT NULL DEFAULT 0)");
    }
};
