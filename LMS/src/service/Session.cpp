#include "service/Session.hpp"

#include <random>

#include "util/Exceptions.hpp"

namespace lms {

SessionManager& SessionManager::getInstance() {
    static SessionManager m;
    return m;
}

const Session& SessionManager::open(std::shared_ptr<const User> user) {
    static const char* hx = "0123456789abcdef";
    std::random_device rd;
    std::string token;
    for (int i = 0; i < 32; ++i) token += hx[rd() % 16];
    session_ = std::make_unique<Session>(std::move(user), token);
    return *session_;
}

Session& SessionManager::current() {
    if (!session_) throw AuthorizationException("You are not logged in.");
    return *session_;
}

}  // namespace lms
