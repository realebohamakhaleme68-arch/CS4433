// AuditLogger: one entry for every mark change (FR-14). Callers invoke it inside
// the same Transaction as the change, so both succeed or both roll back.
#pragma once
#include <string>

#include "data/Repositories.hpp"

namespace lms {

class AuditLogger {
public:
    explicit AuditLogger(IAuditRepository& repo) : repo_(repo) {}
    void log(int userId, int markId, const std::string& action, const std::string& oldValue,
             const std::string& newValue, const std::string& reason);
private:
    IAuditRepository& repo_;
};

}  // namespace lms
