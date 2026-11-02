#include "domain/UserFactory.hpp"

namespace lms {

std::unique_ptr<User> UserFactory::create(Role role, int id, const std::string& un, const std::string& fn,
                                          const std::string& ref, std::optional<int> dept, bool active,
                                          const std::string& created) {
    switch (role) {
        case Role::Admin:
            return std::make_unique<Administrator>(id, un, fn, active, created);
        case Role::Student:
            return std::make_unique<Student>(id, un, fn, active, created, ref);
        case Role::Lecturer:
            if (!dept) throw ValidationException("A lecturer must belong to a department.");
            return std::make_unique<Lecturer>(id, un, fn, active, created, ref, *dept);
        case Role::HoD:
            if (!dept) throw ValidationException("A Head of Department must belong to a department.");
            return std::make_unique<HeadOfDepartment>(id, un, fn, active, created, ref, *dept);
    }
    throw ValidationException("Unknown role.");
}

}  // namespace lms
