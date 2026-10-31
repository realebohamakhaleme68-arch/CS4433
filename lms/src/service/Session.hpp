// Session and SessionManager (Singleton). The session lives in memory only and
// ends at logout or after 15 minutes without activity.
#pragma once
#include <chrono>
#include <memory>
#include <string>

#include "domain/User.hpp"

namespace lms {

class Session {
public:
    using Clock = std::chrono::steady_clock;
    Session(std::shared_ptr<const User> user, std::string token)
        : user_(std::move(user)), token_(std::move(token)), loginTime_(Clock::now()), lastActivity_(Clock::now()) {}

    const User& user() const { return *user_; }
    const std::string& token() const { return token_; }
    void touch() { lastActivity_ = Clock::now(); }
    bool isExpired(std::chrono::minutes idle = std::chrono::minutes(15)) const {
        return Clock::now() - lastActivity_ > idle;
    }

private:
    std::shared_ptr<const User> user_;
    std::string token_;
    Clock::time_point loginTime_;
    Clock::time_point lastActivity_;
};

class SessionManager {
public:
    static SessionManager& getInstance();
    const Session& open(std::shared_ptr<const User> user);
    bool hasSession() const { return session_ != nullptr; }
    Session& current();
    void close() { session_.reset(); }
private:
    SessionManager() = default;
    std::unique_ptr<Session> session_;
};

}  // namespace lms
