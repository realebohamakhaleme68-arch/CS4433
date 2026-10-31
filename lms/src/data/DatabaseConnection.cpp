#include "data/DatabaseConnection.hpp"

#include <sqlite3.h>

#include "schema_sql.hpp"
#include "util/Exceptions.hpp"

namespace lms {

DatabaseConnection& DatabaseConnection::getInstance() {
    static DatabaseConnection instance;
    return instance;
}

void DatabaseConnection::open(const std::string& path) {
    close();
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
        close();
        throw DatabaseException("Cannot open database '" + path + "': " + msg);
    }
    execute("PRAGMA foreign_keys = ON;");
}

void DatabaseConnection::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    depth_ = 0;
    failed_ = false;
}

void DatabaseConnection::execute(const std::string& sql) {
    if (!db_) throw DatabaseException("Database is not open.");
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw DatabaseException("Database error: " + msg);
    }
}

void DatabaseConnection::initialiseSchema() { execute(kSchemaSql); }

long long DatabaseConnection::lastInsertId() const { return sqlite3_last_insert_rowid(db_); }

void DatabaseConnection::beginTransaction() {
    if (depth_ == 0) {
        execute("BEGIN IMMEDIATE;");
        failed_ = false;
    }
    ++depth_;
}
void DatabaseConnection::commit() {
    if (depth_ == 0) return;
    --depth_;
    if (depth_ == 0) execute(failed_ ? "ROLLBACK;" : "COMMIT;");
}
void DatabaseConnection::rollback() {
    if (depth_ == 0) return;
    failed_ = true;
    --depth_;
    if (depth_ == 0) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        failed_ = false;
    }
}

Transaction::Transaction() { DatabaseConnection::getInstance().beginTransaction(); }
Transaction::~Transaction() {
    if (!done_) DatabaseConnection::getInstance().rollback();
}
void Transaction::commit() {
    done_ = true;
    DatabaseConnection::getInstance().commit();
}

// ---------------- Statement ----------------
namespace {
sqlite3* handle() {
    sqlite3* h = DatabaseConnection::getInstance().getHandle();
    if (!h) throw DatabaseException("Database is not open.");
    return h;
}
}  // namespace

Statement::Statement(const std::string& sql) {
    if (sqlite3_prepare_v2(handle(), sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK)
        throw DatabaseException(std::string("Database error: ") + sqlite3_errmsg(handle()));
}
Statement::~Statement() {
    if (stmt_) sqlite3_finalize(stmt_);
}
Statement& Statement::bind(int i, int v) { sqlite3_bind_int(stmt_, i, v); return *this; }
Statement& Statement::bind(int i, long long v) { sqlite3_bind_int64(stmt_, i, v); return *this; }
Statement& Statement::bind(int i, double v) { sqlite3_bind_double(stmt_, i, v); return *this; }
Statement& Statement::bind(int i, const std::string& v) {
    sqlite3_bind_text(stmt_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
    return *this;
}
Statement& Statement::bindNull(int i) { sqlite3_bind_null(stmt_, i); return *this; }
Statement& Statement::bind(int i, const std::optional<int>& v) { return v ? bind(i, *v) : bindNull(i); }
Statement& Statement::bind(int i, const std::optional<std::string>& v) { return v ? bind(i, *v) : bindNull(i); }

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    int ext = sqlite3_extended_errcode(handle());
    std::string msg = sqlite3_errmsg(handle());
    if ((ext & 0xff) == SQLITE_CONSTRAINT)
        throw ValidationException("That change is not allowed (duplicate or invalid reference): " + msg);
    throw DatabaseException("Database error: " + msg);
}
void Statement::run() { step(); }

int Statement::colInt(int c) const { return sqlite3_column_int(stmt_, c); }
double Statement::colDouble(int c) const { return sqlite3_column_double(stmt_, c); }
std::string Statement::colText(int c) const {
    const unsigned char* t = sqlite3_column_text(stmt_, c);
    return t ? reinterpret_cast<const char*>(t) : "";
}
bool Statement::colIsNull(int c) const { return sqlite3_column_type(stmt_, c) == SQLITE_NULL; }
std::optional<int> Statement::colOptInt(int c) const {
    return colIsNull(c) ? std::nullopt : std::optional<int>(colInt(c));
}
std::optional<std::string> Statement::colOptText(int c) const {
    return colIsNull(c) ? std::nullopt : std::optional<std::string>(colText(c));
}

}  // namespace lms
