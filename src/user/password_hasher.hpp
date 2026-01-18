#pragma once
#include <iostream>
#include <random>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>

namespace social_net_service
{
    class PasswordHasher
    {
    public:
        static std::pair<std::string, std::string> HashPassword(
            const std::string& password, size_t iterations = 10000);
        static bool VerifyPassword(const std::string& password,
                                   const std::string& salt,
                                   const std::string& expected_hash,
                                   size_t iterations = 10000);

        static std::string ToHex(const std::string& data);
        static std::string FromHex(const std::string& hex);

    private:
        // Вспомогательная функция для замедленного хеширования
        static std::string SlowHash(const std::string& input, size_t iterations);
        // Генерация "случайной" соли на основе времени
        static std::string GenerateSalt(size_t length = 16);

        static bool ConstantTimeCompare(const std::string& a, const std::string& b);

};

} // social_net_service