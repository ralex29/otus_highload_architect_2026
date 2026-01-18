#pragma once

#include <userver/server/handlers/auth/auth_checker_factory.hpp>

#include "user_info_cache.hpp"

namespace social_net_service
{
    class CheckerFactory final : public userver::server::handlers::auth::AuthCheckerFactoryBase {
    public:
        static constexpr std::string_view kAuthType = "bearer";

        explicit CheckerFactory(const userver::components::ComponentContext& context);

        userver::server::handlers::auth::AuthCheckerBasePtr MakeAuthChecker(
            const userver::server::handlers::auth::HandlerAuthConfig& auth_config
        ) const override;

    private:
        AuthCache& auth_cache_;
    };
} // social_net_service

