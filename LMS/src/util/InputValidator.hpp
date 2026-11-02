// Central input validation (NFR-5, password policy from RE ambiguity 2).
#pragma once
#include <string>

namespace lms {

class InputValidator {
public:
    static bool isStrongPassword(const std::string& pw);   // >= 8 chars: letters + digits + symbols
    static bool isValidUsername(const std::string& u);     // 3-32 chars: letters, digits, . _ -
    static void requireNonEmpty(const std::string& value, const std::string& fieldName);
    static void requireMark(double mark, double maxMark);  // 0 <= mark <= maxMark
    static void validateSubmissionFile(const std::string& path);  // exists, not empty, <= 10 MB, allowed type
    static int parseInt(const std::string& text, const std::string& fieldName);
    static double parseDouble(const std::string& text, const std::string& fieldName);
    static constexpr long long kMaxFileBytes = 10LL * 1024 * 1024;
};

}  // namespace lms
