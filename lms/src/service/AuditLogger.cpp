#include "service/AuditLogger.hpp"

#include "util/InputValidator.hpp"

namespace lms {

void AuditLogger::log(int userId, int markId, const std::string& action, const std::string& oldValue,
                      const std::string& newValue, const std::string& reason) {
    InputValidator::requireNonEmpty(reason, "Reason");
    AuditLogEntry e;
    e.markId = markId;
    e.userId = userId;
    e.actionType = action;
    if (!oldValue.empty()) e.oldValue = oldValue;
    if (!newValue.empty()) e.newValue = newValue;
    e.reason = reason;
    repo_.append(e);
}

}  // namespace lms
