#include "../Include/utils/PasswordHasher.h"


#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>


#include <array>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <charconv>
#include <climits>

using namespace std;
namespace {
// Password-hash parameters are stored with each hash so verification can reproduce them.
constexpr int ITERATIONS = 310000;
constexpr int SALT_SIZE = 16;
constexpr int HASH_SIZE = 32;

constexpr const char* ALGORITHM = "pbkdf2_sha256";

// Encodes binary salts and hashes as hexadecimal text for storage.
string toHex(const unsigned char* data, size_t length) {
    ostringstream out;
    out << hex << setfill('0');
    for (size_t i = 0; i < length; ++i) {
        out << setw(2) << static_cast<unsigned int>(data[i]);
    }
    return out.str();
}

// Decodes stored hexadecimal values and rejects malformed input.
bool fromHex(const string& hex, vector<unsigned char>& bytes) {
    if (hex.empty() || hex.size() % 2 != 0) return false;
    bytes.clear();
    bytes.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned int value = 0;
        auto result = from_chars(hex.data() + i,
                                    hex.data() + i + 2,
                                    value,
                                    16);
        if (result.ec != errc{} || value > 255) return false;
        bytes.push_back(static_cast<unsigned char>(value));
    }
    return true;
}

// Derives PBKDF2-SHA256 bytes for both password creation and verification.
bool deriveHash(const string& password,
               const unsigned char* salt,
               int saltLength,
               int iterations,
               unsigned char* output,
               int outputLength) {
    if (password.size() > static_cast<size_t>(INT_MAX)) return false;
    return PKCS5_PBKDF2_HMAC(password.data(),
                             static_cast<int>(password.size()),
                             salt,
                             saltLength,
                             iterations,
                             EVP_sha256(),
                             outputLength,
                             output) == 1;
}

}

// Creates a salted PBKDF2 password hash.
string PasswordHasher::hashPassword(const string& password) {
    array<unsigned char, SALT_SIZE> salt{};
    array<unsigned char, HASH_SIZE> hash{};

    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        return {};
    }

    if (!deriveHash(password, salt.data(), static_cast<int>(salt.size()),
                    ITERATIONS, hash.data(), static_cast<int>(hash.size()))) {
        return {};
    }

    return string(ALGORITHM) + "$" + to_string(ITERATIONS) + "$" +
           toHex(salt.data(), salt.size()) + "$" + toHex(hash.data(), hash.size());
}

// Stores reset tokens as SHA-256 digests so the database has no redeemable plaintext token.
string PasswordHasher::hashToken(const string& token) {
    array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;
    if (EVP_Digest(token.data(), token.size(), digest.data(), &digestLength,
                   EVP_sha256(), nullptr) != 1) {
        return {};
    }
    return toHex(digest.data(), digestLength);
}

// Validates the encoded parameters and compares derived hashes in constant time.
bool PasswordHasher::verifyPassword(const string& password,
                                    const string& hashedPassword) {
    // Expected format: algorithm$iterations$salt_hex$hash_hex
    istringstream input(hashedPassword);
    string algorithm, iterationText, saltHex, hashHex, extra;

    if (!getline(input, algorithm, '$') ||
        !getline(input, iterationText, '$') ||
        !getline(input, saltHex, '$') ||
        !getline(input, hashHex) ||
        getline(input, extra)) {
        return false;
    }

    if (algorithm != ALGORITHM) return false;

    int iterations = 0;
    auto parseResult = from_chars(iterationText.data(),
                                      iterationText.data() + iterationText.size(),
                                      iterations);
    if (parseResult.ec != errc{} ||
        parseResult.ptr != iterationText.data() + iterationText.size() ||
        iterations < 10000 || iterations > 2000000) {
        return false;
    }

    vector<unsigned char> salt;
    vector<unsigned char> storedHash;
    if (!fromHex(saltHex, salt) || !fromHex(hashHex, storedHash)) return false;
    if (salt.size() != SALT_SIZE || storedHash.size() != HASH_SIZE) return false;

    array<unsigned char, HASH_SIZE> calculated{};
    if (!deriveHash(password, salt.data(), static_cast<int>(salt.size()),
                    iterations, calculated.data(), static_cast<int>(calculated.size()))) {
        return false;
    }

    // Constant-time comparison to reduce timing leakage.
    return CRYPTO_memcmp(calculated.data(), storedHash.data(), HASH_SIZE) == 0;
}

// Generates reset-token characters from cryptographically secure random bytes.
string PasswordHasher::randomString(size_t length, bool includeDigits) {
    const string alphabet = includeDigits
        ? "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        : "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (length == 0)
        return {};
    vector<unsigned char> bytes(length);
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
        return {};
    string result;
    result.reserve(length);
    for (unsigned char byte : bytes)
        result.push_back(alphabet[byte % alphabet.size()]);
    return result;
}
