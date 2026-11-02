// DatabaseConnection (Singleton), Transaction (RAII) and Statement (bound-parameter
// wrapper). Only the data-access tier includes this header; every query goes
// through Statement with bound parameters (NFR-4).
#pragma once
#include <optional>
#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace lms {

class DatabaseConnection {
public:
    static DatabaseConnection& getInstance();
    DatabaseConnection(const DatabaseConnection&) = delete;
    DatabaseConnection& operator=(const DatabaseConnection&) = delete;

    void open(const std::string& path);   // also enables PRAGMA foreign_keys
    void close();
    bool isOpen() const { return db_ != nullptr; }
    sqlite3* getHandle() const { return db_; }

    void initialiseSchema();              // runs the embedded db/schema.sql (idempotent)
    void execute(const std::string& sql); // for fixed SQL without user data only
    long long lastInsertId() const;

    // Nested transactions: only the outermost begin/commit touches SQLite.
    void beginTransaction();
    void commit();
    void rollback();

private:
    DatabaseConnection() = default;
    ~DatabaseConnection() { close(); }
    sqlite3* db_ = nullptr;
    int depth_ = 0;
    bool failed_ = false;
};

class Transaction {
public:
    Transaction();
    ~Transaction();
    void commit();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
private:
    bool done_ = false;
};

class Statement {
public:
    explicit Statement(const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement& bind(int index, int v);
    Statement& bind(int index, long long v);
    Statement& bind(int index, double v);
    Statement& bind(int index, const std::string& v);
    Statement& bind(int index, const char* v) { return bind(index, std::string(v)); }
    Statement& bindNull(int index);
    Statement& bind(int index, const std::optional<int>& v);
    Statement& bind(int index, const std::optional<std::string>& v);

    bool step();   // true = a row is available, false = done
    void run();    // step() for statements that return no rows

    int colInt(int c) const;
    double colDouble(int c) const;
    std::string colText(int c) const;
    bool colIsNull(int c) const;
    std::optional<int> colOptInt(int c) const;
    std::optional<std::string> colOptText(int c) const;

private:
    sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace lms
