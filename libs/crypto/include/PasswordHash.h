#pragma once

#include <cstdint>
#include <string>
#include <vector>

class PasswordHash {
public:
    static std::string hash(const std::string& password);
    static bool verify(const std::string& password, const std::string& hashed);
    
    static constexpr size_t SALT_SIZE = 16;
    static constexpr size_t HASH_SIZE = 32;
    static constexpr uint32_t ITERATIONS = 100000;

private:
    static std::vector<uint8_t> generateSalt();
    static std::vector<uint8_t> pbkdf2(const std::string& password, const std::vector<uint8_t>& salt);
    static std::vector<uint8_t> hmacSha256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);
    static std::vector<uint8_t> sha256(const std::vector<uint8_t>& data);
    static std::string toHex(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> fromHex(const std::string& hex);
};