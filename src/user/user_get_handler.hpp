#pragma once

#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

#include <userver/storages/postgres/cluster.hpp>

namespace social_net_service::user
{
    class UserGetHandler final : public userver::server::handlers::HttpHandlerBase
    {
    public:
        static constexpr std::string_view kName = "handler-user-get";

        UserGetHandler(const userver::components::ComponentConfig&, const userver::components::ComponentContext&);

        std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
                                       userver::server::request::RequestContext&)
        const override;

    private:
        userver::storages::postgres::ClusterPtr pg_cluster_;
    };
} // social_net_service::user
