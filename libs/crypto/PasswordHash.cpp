#include "PasswordHash.h"

#if defined(_WIN32)
    #include <windows.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#endif

#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <fstream>

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ((w[i - 15] >> 7) | (w[i - 15] << 25)) ^ ((w[i - 15] >> 18) | (w[i - 15] << 14)) ^ (w[i - 15] >> 3);
        uint32_t s1 = ((w[i - 2] >> 17) | (w[i - 2] << 15)) ^ ((w[i - 2] >> 19) | (w[i - 2] << 13)) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t s1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

std::vector<uint8_t> PasswordHash::sha256(const std::vector<uint8_t>& data) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    size_t len = data.size();
    size_t padded_len = ((len + 8) / 64 + 1) * 64;
    std::vector<uint8_t> padded(padded_len, 0);
    std::memcpy(padded.data(), data.data(), len);
    padded[len] = 0x80;

    uint64_t bit_len = static_cast<uint64_t>(len) * 8;
    for (int i = 0; i < 8; ++i) {
        padded[padded_len - 8 + i] = static_cast<uint8_t>(bit_len >> (56 - i * 8));
    }

    for (size_t i = 0; i < padded_len; i += 64) {
        sha256_transform(state, padded.data() + i);
    }

    std::vector<uint8_t> hash(32);
    for (int i = 0; i < 8; ++i) {
        hash[i * 4] = static_cast<uint8_t>(state[i] >> 24);
        hash[i * 4 + 1] = static_cast<uint8_t>(state[i] >> 16);
        hash[i * 4 + 2] = static_cast<uint8_t>(state[i] >> 8);
        hash[i * 4 + 3] = static_cast<uint8_t>(state[i]);
    }
    return hash;
}

std::vector<uint8_t> PasswordHash::hmacSha256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> k = key;
    if (k.size() > 64) {
        k = sha256(k);
    }
    k.resize(64, 0);

    std::vector<uint8_t> ipad(64), opad(64);
    for (size_t i = 0; i < 64; ++i) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    std::vector<uint8_t> inner = ipad;
    inner.insert(inner.end(), data.begin(), data.end());
    std::vector<uint8_t> inner_hash = sha256(inner);

    std::vector<uint8_t> outer = opad;
    outer.insert(outer.end(), inner_hash.begin(), inner_hash.end());
    return sha256(outer);
}

std::vector<uint8_t> PasswordHash::generateSalt() {
    std::vector<uint8_t> salt(SALT_SIZE);
#if defined(_WIN32)
    BCRYPT_ALG_HANDLE hAlgorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_RNG_ALGORITHM, nullptr, 0) != 0) {
        throw std::runtime_error("Failed to open RNG algorithm");
    }
    if (BCryptGenRandom(hAlgorithm, salt.data(), static_cast<ULONG>(salt.size()), 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlgorithm, 0);
        throw std::runtime_error("Failed to generate random salt");
    }
    BCryptCloseAlgorithmProvider(hAlgorithm, 0);
#elif defined(__ANDROID__) || defined(__linux__) || defined(__APPLE__)
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (urandom) {
        urandom.read(reinterpret_cast<char*>(salt.data()), salt.size());
        if (!urandom) {
            throw std::runtime_error("Failed to read from /dev/urandom");
        }
    } else {
        throw std::runtime_error("Failed to open /dev/urandom");
    }
#else
    std::random_device rd;
    for (size_t i = 0; i < salt.size(); ++i) {
        salt[i] = static_cast<uint8_t>(rd());
    }
#endif
    return salt;
}

std::vector<uint8_t> PasswordHash::pbkdf2(const std::string& password, const std::vector<uint8_t>& salt) {
    std::vector<uint8_t> hash(HASH_SIZE);
    
    std::vector<uint8_t> pass_bytes(password.begin(), password.end());
    size_t blocks = (HASH_SIZE + 31) / 32;
    
    for (size_t block_idx = 1; block_idx <= blocks; ++block_idx) {
        std::vector<uint8_t> data = salt;
        data.push_back(static_cast<uint8_t>(block_idx >> 24));
        data.push_back(static_cast<uint8_t>(block_idx >> 16));
        data.push_back(static_cast<uint8_t>(block_idx >> 8));
        data.push_back(static_cast<uint8_t>(block_idx));

        std::vector<uint8_t> u = hmacSha256(pass_bytes, data);
        std::vector<uint8_t> t = u;

        for (uint32_t i = 1; i < ITERATIONS; ++i) {
            u = hmacSha256(pass_bytes, u);
            for (size_t j = 0; j < u.size(); ++j) {
                t[j] ^= u[j];
            }
        }

        size_t offset = (block_idx - 1) * 32;
        size_t copy_len = std::min<size_t>(32, hash.size() - offset);
        std::memcpy(hash.data() + offset, t.data(), copy_len);
    }
    
    return hash;
}

std::string PasswordHash::toHex(const std::vector<uint8_t>& data) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(data.size() * 2);
    for (uint8_t byte : data) {
        result.push_back(hex[byte >> 4]);
        result.push_back(hex[byte & 0xf]);
    }
    return result;
}

std::vector<uint8_t> PasswordHash::fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::invalid_argument("Invalid hex string length");
    }
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        char c1 = hex[i];
        char c2 = hex[i + 1];
        uint8_t byte = 0;
        if (c1 >= '0' && c1 <= '9') byte |= (c1 - '0') << 4;
        else if (c1 >= 'a' && c1 <= 'f') byte |= (c1 - 'a' + 10) << 4;
        else if (c1 >= 'A' && c1 <= 'F') byte |= (c1 - 'A' + 10) << 4;
        else throw std::invalid_argument("Invalid hex character");
        if (c2 >= '0' && c2 <= '9') byte |= (c2 - '0');
        else if (c2 >= 'a' && c2 <= 'f') byte |= (c2 - 'a' + 10);
        else if (c2 >= 'A' && c2 <= 'F') byte |= (c2 - 'A' + 10);
        else throw std::invalid_argument("Invalid hex character");
        result.push_back(byte);
    }
    return result;
}

std::string PasswordHash::hash(const std::string& password) {
    auto salt = generateSalt();
    auto derived = pbkdf2(password, salt);
    std::string result;
    result.reserve(salt.size() * 2 + derived.size() * 2 + 1);
    result = toHex(salt) + ":" + toHex(derived);
    return result;
}

bool PasswordHash::verify(const std::string& password, const std::string& hashed) {
    auto colonPos = hashed.find(':');
    if (colonPos == std::string::npos) {
        return false;
    }
    auto salt = fromHex(hashed.substr(0, colonPos));
    auto expectedHash = fromHex(hashed.substr(colonPos + 1));
    if (expectedHash.size() != HASH_SIZE) {
        return false;
    }
    auto derived = pbkdf2(password, salt);
    return derived == expectedHash;
}