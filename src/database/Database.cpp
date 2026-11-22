#include "../Include/database/Database.h"
using namespace std;

namespace {
// Converts SQLite failures into exceptions with useful database diagnostics.
[[noreturn]] void throwError(sqlite3* db, const string& what) {
    throw DatabaseException(what + ": " + (db ? sqlite3_errmsg(db) : "unknown error"));
}
}

// Statement setup, cleanup, and binding keep SQL parameters safe and reusable.
Statement::Statement(sqlite3* db, const string& sql) {
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK)
        throwError(db, "Prepare failed [" + sql + "]");
}

Statement::~Statement() { if (stmt_) sqlite3_finalize(stmt_); }

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_) { other.stmt_ = nullptr; }

Statement& Statement::bindInt(int index, long long value) {
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) throwError(sqlite3_db_handle(stmt_), "bindInt");
    return *this;
}

Statement& Statement::bindText(int index, const string& value) {
    if (sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK)
        throwError(sqlite3_db_handle(stmt_), "bindText");
    return *this;
}

Statement& Statement::bindNull(int index) {
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) throwError(sqlite3_db_handle(stmt_), "bindNull");
    return *this;
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throwError(sqlite3_db_handle(stmt_), "Step failed");
}

void Statement::execute() { while (step()) {} }

long long Statement::columnInt(int col) const { return sqlite3_column_int64(stmt_, col); }

string Statement::columnText(int col) const {
    const unsigned char* t = sqlite3_column_text(stmt_, col);
    return t ? string(reinterpret_cast<const char*>(t)) : string();
}

// Database owns the SQLite connection and provides execution and transaction helpers.
Database::Database(const string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        string msg = db_ ? sqlite3_errmsg(db_) : "cannot allocate database";
        sqlite3_close(db_);
        db_ = nullptr;
        throw DatabaseException("Cannot open database '" + path + "': " + msg);
    }
    sqlite3_busy_timeout(db_, 5000);
    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA journal_mode = WAL;");
}

Database::~Database() { if (db_) sqlite3_close(db_); }

Statement Database::prepare(const string& sql) { return Statement(db_, sql); }

void Database::execute(const string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw DatabaseException("Execute failed: " + msg);
    }
}

long long Database::lastInsertId() const { return sqlite3_last_insert_rowid(db_); }

// Nested operations share one outer SQLite transaction.
void Database::begin() {
    if (depth_ == 0) execute("BEGIN IMMEDIATE;");
    ++depth_;
}

void Database::commit() {
    if (depth_ == 0) throw DatabaseException("commit without active transaction");
    if (--depth_ == 0) execute("COMMIT;");
}

void Database::rollback() {
    if (depth_ == 0) return;
    depth_ = 0;
    execute("ROLLBACK;");
}
