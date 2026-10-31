#include "util/PasswordHasher.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include "util/Exceptions.hpp"

#ifdef LMS_USE_LIBSODIUM
#include <sodium.h>
#endif

namespace lms {
namespace {

// ---- Minimal SHA-256 (FIPS 180-4) for the fallback hasher ----
struct Sha256 {
    std::array<uint32_t, 8> h{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                              0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::vector<uint8_t> buf;
    uint64_t total = 0;
    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    void block(const uint8_t* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) | (uint32_t(p[i * 4 + 2]) << 8) |
                   uint32_t(p[i * 4 + 3]);
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25), ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22), mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    void update(const uint8_t* d, size_t n) {
        total += n;
        buf.insert(buf.end(), d, d + n);
        size_t off = 0;
        while (buf.size() - off >= 64) { block(&buf[off]); off += 64; }
        buf.erase(buf.begin(), buf.begin() + static_cast<long>(off));
    }
    std::array<uint8_t, 32> finish() {
        uint64_t bits = total * 8;
        buf.push_back(0x80);
        while (buf.size() % 64 != 56) buf.push_back(0);
        for (int i = 7; i >= 0; --i) buf.push_back(static_cast<uint8_t>(bits >> (i * 8)));
        for (size_t off = 0; off < buf.size(); off += 64) block(&buf[off]);
        std::array<uint8_t, 32> out{};
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 4; ++j) out[i * 4 + j] = static_cast<uint8_t>(h[i] >> (24 - 8 * j));
        return out;
    }
};

constexpr int kIterations = 100000;
const char* kPrefix = "$lmsfb$";

std::string toHex(const uint8_t* d, size_t n) {
    static const char* hx = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) { s += hx[d[i] >> 4]; s += hx[d[i] & 15]; }
    return s;
}
std::vector<uint8_t> fromHex(const std::string& s) {
    std::vector<uint8_t> v;
    auto nib = [](char c) { return c >= 'a' ? c - 'a' + 10 : c - '0'; };
    for (size_t i = 0; i + 1 < s.size(); i += 2) v.push_back(static_cast<uint8_t>(nib(s[i]) * 16 + nib(s[i + 1])));
    return v;
}
std::array<uint8_t, 32> derive(const std::vector<uint8_t>& salt, const std::string& pw, int iters) {
    Sha256 s;
    s.update(salt.data(), salt.size());
    s.update(reinterpret_cast<const uint8_t*>(pw.data()), pw.size());
    auto h = s.finish();
    for (int i = 0; i < iters; ++i) {
        Sha256 t;
        t.update(h.data(), h.size());
        t.update(salt.data(), salt.size());
        t.update(reinterpret_cast<const uint8_t*>(pw.data()), pw.size());
        h = t.finish();
    }
    return h;
}
bool constantTimeEq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char r = 0;
    for (size_t i = 0; i < a.size(); ++i) r |= static_cast<unsigned char>(a[i] ^ b[i]);
    return r == 0;
}
}  // namespace

bool PasswordHasher::usesArgon2id() {
#ifdef LMS_USE_LIBSODIUM
    return true;
#else
    return false;
#endif
}

std::string PasswordHasher::hashPassword(const std::string& password) {
#ifdef LMS_USE_LIBSODIUM
    if (sodium_init() < 0) throw AppException("Could not initialise libsodium");
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(out, password.c_str(), password.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
        throw AppException("Password hashing failed (out of memory)");
    return out;
#else
    std::random_device rd;
    std::vector<uint8_t> salt(16);
    for (auto& b : salt) b = static_cast<uint8_t>(rd());
    auto h = derive(salt, password, kIterations);
    return std::string(kPrefix) + std::to_string(kIterations) + "$" + toHex(salt.data(), salt.size()) + "$" +
           toHex(h.data(), h.size());
#endif
}

bool PasswordHasher::verifyPassword(const std::string& stored, const std::string& password) {
#ifdef LMS_USE_LIBSODIUM
    if (stored.rfind("$argon2", 0) == 0) {
        if (sodium_init() < 0) return false;
        return crypto_pwhash_str_verify(stored.c_str(), password.c_str(), password.size()) == 0;
    }
#endif
    if (stored.rfind(kPrefix, 0) != 0) return false;  // includes the "!" of the SYSTEM account
    // $lmsfb$<iters>$<salt>$<hash>
    size_t p1 = stored.find('$', 7), p2 = stored.find('$', p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos) return false;
    int iters = std::atoi(stored.substr(7, p1 - 7).c_str());
    if (iters <= 0 || iters > 5000000) return false;
    auto salt = fromHex(stored.substr(p1 + 1, p2 - p1 - 1));
    auto h = derive(salt, password, iters);
    return constantTimeEq(toHex(h.data(), h.size()), stored.substr(p2 + 1));
}

}  // namespace lms
