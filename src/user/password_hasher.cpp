#include "password_hasher.hpp"

namespace social_net_service
{
    std::string PasswordHasher::SlowHash(const std::string& input, size_t iterations)
    {
        std::string hash = input;

        for (size_t i = 0; i < iterations; ++i)
        {
            // Комбинируем несколько методов хеширования
            std::hash<std::string> string_hasher;
            std::hash<size_t> size_hasher;

            size_t h1 = string_hasher(hash);
            size_t h2 = size_hasher(i);
            size_t combined = h1 ^ (h2 << 1);

            // Преобразуем в строку
            hash = std::to_string(combined);

            // Добавляем задержку для замедления
            if (i % 100 == 0)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }

        return hash;
    }

    // Генерация "случайной" соли на основе времени
    std::string PasswordHasher::GenerateSalt(size_t length)
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();

        // Используем несколько источников энтропии
        static std::atomic<size_t> counter{0};
        size_t count = counter.fetch_add(1, std::memory_order_relaxed);

        // Создаем seed из времени и счетчика
        std::seed_seq seed{
            static_cast<uint32_t>(nanos & 0xFFFFFFFF),
            static_cast<uint32_t>((nanos >> 32) & 0xFFFFFFFF),
            static_cast<uint32_t>(count),
            static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()))
        };

        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<int> dist(0, 255);

        std::string salt;
        salt.reserve(length);

        for (size_t i = 0; i < length; ++i)
        {
            salt.push_back(static_cast<char>(dist(rng)));
        }

        return salt;
    }

    bool PasswordHasher::ConstantTimeCompare(const std::string& a, const std::string& b)
    {
        if (a.length() != b.length())
        {
            return false;
        }

        char result = 0;
        for (size_t i = 0; i < a.length(); ++i)
        {
            result |= a[i] ^ b[i];
        }

        return result == 0;
    }

    std::pair<std::string, std::string> PasswordHasher::HashPassword(
        const std::string& password, size_t iterations)
    {
        // Генерация соли
        std::string salt = GenerateSalt();

        // Комбинируем пароль и соль
        std::string combined = password + salt;

        // Замедленное хеширование
        std::string hash = SlowHash(combined, iterations);

        return {salt, hash};
    }

    bool PasswordHasher::VerifyPassword(const std::string& password,
                                        const std::string& salt,
                                        const std::string& expected_hash,
                                        size_t iterations)
    {
        std::string combined = password + salt;
        std::string actual_hash = SlowHash(combined, iterations);

        return ConstantTimeCompare(actual_hash, expected_hash);
    }

    std::string PasswordHasher::ToHex(const std::string& data)
    {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');

        for (unsigned char c : data)
        {
            ss << std::setw(2) << static_cast<int>(c);
        }

        return ss.str();
    }

    std::string PasswordHasher::FromHex(const std::string& hex)
    {
        std::string result;

        if (hex.length() % 2 != 0) {
            throw std::invalid_argument("Invalid hex string length");
        }

        std::stringstream ss;
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            int byteValue;
            ss.clear();
            ss << std::hex << byteString;
            ss >> byteValue;
            result.push_back(static_cast<char>(byteValue));
        }

        return result;
    }

} // social_net_service
