// Exception hierarchy (design Fig 2E). Controllers catch AppException so that
// invalid input or a database error never crashes the program (NFR-5).
#pragma once
#include <stdexcept>
#include <string>

namespace lms {

class AppException : public std::runtime_error {
public:
    explicit AppException(const std::string& m) : std::runtime_error(m) {}
};
class ValidationException : public AppException {
public:
    using AppException::AppException;
};
class AuthorizationException : public AppException {
public:
    using AppException::AppException;
};
class DatabaseException : public AppException {
public:
    using AppException::AppException;
};

}  // namespace lms
