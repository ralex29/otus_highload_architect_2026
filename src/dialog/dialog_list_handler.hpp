#pragma once

#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/redis/client.hpp>

namespace social_net_service::dialog
{
    class DialogListHandler final : public userver::server::handlers::HttpHandlerBase
    {
    public:
        static constexpr std::string_view kName = "handler-dialog-list";

        DialogListHandler(const userver::components::ComponentConfig&, const userver::components::ComponentContext&);

        std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
                                       userver::server::request::RequestContext&) const override;

    private:
        std::shared_ptr<userver::storages::redis::Client> redis_client_;
    };
} // social_net_service::dialog
