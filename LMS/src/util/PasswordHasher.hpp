// Password hashing (NFR-3). Salted hashes only, never plain text.
//  * With libsodium (LMS_USE_LIBSODIUM): Argon2id via crypto_pwhash_str. The salt
//    and cost parameters are embedded in the stored string.
//  * Without libsodium: salted, iterated SHA-256 ("$lmsfb$..." format). This keeps
//    the project buildable everywhere; use libsodium for a real deployment.
#pragma once
#include <string>

namespace lms {

class PasswordHasher {
public:
    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& stored, const std::string& password);
    static bool usesArgon2id();
};

}  // namespace lms
