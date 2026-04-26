#pragma once

#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>

#include "dialog_grpc_client.hpp"

namespace social_net_service::dialog
{
    class DialogSendHandler final : public userver::server::handlers::HttpHandlerJsonBase
    {
    public:
        static constexpr std::string_view kName = "handler-dialog-send";

        DialogSendHandler(const userver::components::ComponentConfig&, const userver::components::ComponentContext&);

        userver::formats::json::Value HandleRequestJsonThrow(
            const userver::server::http::HttpRequest& request,
            const userver::formats::json::Value& request_json,
            userver::server::request::RequestContext& context) const override;

    private:
        chat::v1::DialogServiceClient& grpc_client_;
    };
} // social_net_service::dialog
