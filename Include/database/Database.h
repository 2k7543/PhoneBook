#pragma once
#include <sqlite3.h>
#include <stdexcept>
#include <string>
using namespace std;

class DatabaseException : public runtime_error {
public:
    using runtime_error::runtime_error;
};

class Statement {
public:
    Statement(sqlite3* db, const string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&&) = delete;

    Statement& bindInt(int index, long long value);
    Statement& bindText(int index, const string& value);
    Statement& bindNull(int index);

    bool step();
    void execute();

    long long columnInt(int col) const;
    bool columnBool(int col) const { return columnInt(col) != 0; }
    string columnText(int col) const;

private:
    sqlite3_stmt* stmt_ = nullptr;
};

class Database {
public:
    explicit Database(const string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Statement prepare(const string& sql);
    void execute(const string& sql);
    long long lastInsertId() const;

    void begin();
    void commit();
    void rollback();
    int transactionDepth() const { return depth_; }

private:
    sqlite3* db_ = nullptr;
    int depth_ = 0;
};

class Transaction {
public:
    explicit Transaction(Database& db) : db_(db) { db_.begin(); }
    ~Transaction() {
        if (!done_) {
            try { db_.rollback(); } catch (...) {}
        }
    }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    void commit() { db_.commit(); done_ = true; }

private:
    Database& db_;
    bool done_ = false;
};
