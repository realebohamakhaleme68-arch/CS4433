#include "util/InputValidator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <set>
#include <system_error>

#include "util/Exceptions.hpp"

namespace lms {

bool InputValidator::isStrongPassword(const std::string& pw) {
    if (pw.size() < 8 || pw.size() > 128) return false;
    bool letter = false, digit = false, symbol = false;
    for (unsigned char c : pw) {
        if (std::isalpha(c)) letter = true;
        else if (std::isdigit(c)) digit = true;
        else if (std::ispunct(c)) symbol = true;
    }
    return letter && digit && symbol;
}

bool InputValidator::isValidUsername(const std::string& u) {
    if (u.size() < 3 || u.size() > 32) return false;
    return std::all_of(u.begin(), u.end(), [](unsigned char c) { return std::isalnum(c) || c == '.' || c == '_' || c == '-'; });
}

void InputValidator::requireNonEmpty(const std::string& v, const std::string& field) {
    if (std::all_of(v.begin(), v.end(), [](unsigned char c) { return std::isspace(c); }))
        throw ValidationException(field + " cannot be empty.");
}

void InputValidator::requireMark(double mark, double maxMark) {
    if (std::isnan(mark) || mark < 0 || mark > maxMark)
        throw ValidationException("Mark must be between 0 and " + std::to_string(static_cast<int>(maxMark)) + ".");
}

void InputValidator::validateSubmissionFile(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path p(path);
    if (!fs::is_regular_file(p, ec)) throw ValidationException("File not found: " + path);
    auto size = fs::file_size(p, ec);
    if (ec || size == 0) throw ValidationException("The file is empty or unreadable.");
    if (static_cast<long long>(size) > kMaxFileBytes) throw ValidationException("The file is larger than 10 MB.");
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    static const std::set<std::string> allowed = {".pdf", ".doc", ".docx", ".txt", ".zip", ".cpp", ".hpp",
                                                  ".c",   ".h",   ".py",   ".java", ".odt", ".rtf", ".md"};
    if (!allowed.count(ext))
        throw ValidationException("File type not allowed (use pdf, doc, docx, txt, zip, odt, rtf, md or source files).");
}

int InputValidator::parseInt(const std::string& t, const std::string& f) {
    try {
        size_t pos = 0;
        int v = std::stoi(t, &pos);
        if (pos != t.size()) throw std::invalid_argument("x");
        return v;
    } catch (...) {
        throw ValidationException(f + " must be a whole number.");
    }
}
double InputValidator::parseDouble(const std::string& t, const std::string& f) {
    try {
        size_t pos = 0;
        double v = std::stod(t, &pos);
        if (pos != t.size() || std::isnan(v) || std::isinf(v)) throw std::invalid_argument("x");
        return v;
    } catch (...) {
        throw ValidationException(f + " must be a number.");
    }
}

}  // namespace lms
