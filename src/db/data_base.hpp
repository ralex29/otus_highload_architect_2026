#pragma once

#include <string_view>

namespace social_net_service
{
    class DataBase final
    {
        public:
        static constexpr std::string_view Name = "postgres-db-1";
        static constexpr std::string_view AuthName = "auth-database";
    };
} // social_net_service
