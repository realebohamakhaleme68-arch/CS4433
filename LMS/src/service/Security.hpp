// Security services: login (FR-1) and role / scope checks (FR-2).
#pragma once
#include <string>

#include "data/Repositories.hpp"
#include "service/Session.hpp"

namespace lms {

class AuthorizationService {
public:
    // Role check: the first statement of every service method.
    void require(const Session& s, Permission p) const;
    // Data-scope checks, applied after the role check.
    void requireDepartment(const Session& s, int departmentId) const;   // HoD: own department only
    void requireOwner(const Session& s, int ownerUserId) const;         // student: own data only
    void requireLecturerOf(const Session& s, const Course& c) const;    // lecturer: own courses only
};

class AuthenticationService {
public:
    explicit AuthenticationService(IUserRepository& users) : users_(users) {}
    // Returns the new session or throws ValidationException with a generic message.
    const Session& authenticate(const std::string& username, const std::string& password);
    void logout();
private:
    IUserRepository& users_;
};

}  // namespace lms
