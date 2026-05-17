#pragma once

#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

#include "counter_grpc_client.hpp"

namespace social_net_service::dialog
{
    class DialogUnreadHandler final : public userver::server::handlers::HttpHandlerBase
    {
    public:
        static constexpr std::string_view kName = "handler-dialog-unread-count";

        DialogUnreadHandler(const userver::components::ComponentConfig&,
                            const userver::components::ComponentContext&);

        std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
                                       userver::server::request::RequestContext&) const override;

    private:
        counter::v1::CounterServiceClient& counter_client_;
    };
} // namespace social_net_service::dialog
