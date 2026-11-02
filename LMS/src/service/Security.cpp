#include "service/Security.hpp"

#include "domain/UserFactory.hpp"
#include "util/Exceptions.hpp"
#include "util/PasswordHasher.hpp"

namespace lms {

void AuthorizationService::require(const Session& s, Permission p) const {
    if (!s.user().can(p))
        throw AuthorizationException("Access denied: your role (" + s.user().roleName() + ") may not perform " +
                                     toString(p) + ".");
}
void AuthorizationService::requireDepartment(const Session& s, int departmentId) const {
    auto d = s.user().departmentId();
    if (!d || *d != departmentId) throw AuthorizationException("Access denied: that item belongs to another department.");
}
void AuthorizationService::requireOwner(const Session& s, int ownerUserId) const {
    if (s.user().getUserId() != ownerUserId) throw AuthorizationException("Access denied: that is not your data.");
}
void AuthorizationService::requireLecturerOf(const Session& s, const Course& c) const {
    if (!c.lecturerId || *c.lecturerId != s.user().getUserId())
        throw AuthorizationException("Access denied: you are not the lecturer of " + c.code + ".");
}

const Session& AuthenticationService::authenticate(const std::string& username, const std::string& password) {
    static const std::string kGeneric = "Invalid username or password.";
    // A dummy hash is verified when the user does not exist so response time does
    // not reveal which usernames are valid.
    static const std::string dummyHash = PasswordHasher::hashPassword("dummy-Password-1!");
    if (username.empty() || username.size() > 64 || password.empty() || password.size() > 128)
        throw ValidationException(kGeneric);
    auto rec = users_.getAuthRecord(username);
    bool ok = PasswordHasher::verifyPassword(rec ? rec->passwordHash : dummyHash, password);
    if (!rec || !rec->active || !ok || rec->userId == kSystemUserId) throw ValidationException(kGeneric);
    std::shared_ptr<const User> user = users_.findById(rec->userId);
    if (!user) throw ValidationException(kGeneric);
    return SessionManager::getInstance().open(user);
}

void AuthenticationService::logout() { SessionManager::getInstance().close(); }

}  // namespace lms
