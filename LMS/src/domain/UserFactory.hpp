// Factory pattern: the only place that switches on the role to build the right
// User subclass. A new role means one new class plus one case here (OCP).
#pragma once
#include <memory>
#include <optional>
#include <string>

#include "domain/User.hpp"

namespace lms {

class UserFactory {
public:
    static std::unique_ptr<User> create(Role role, int id, const std::string& username, const std::string& fullName,
                                        const std::string& referenceNo, std::optional<int> departmentId,
                                        bool active = true, const std::string& createdAt = "");
};

}  // namespace lms
