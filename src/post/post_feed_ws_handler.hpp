#pragma once
#include <userver/components/component.hpp>
#include <userver/server/handlers/websocket_handler.hpp>

namespace social_net_service::post {
class PostFeedWsManager;

class PostFeedWsHandler final
    : public userver::server::handlers::WebsocketHandlerBase {
public:
    static constexpr std::string_view kName = "handler-post-feed-ws";
    PostFeedWsHandler(const userver::components::ComponentConfig&,
                      const userver::components::ComponentContext&);
    void Handle(userver::websocket::WebSocketConnection& ws,
                userver::server::request::RequestContext& ctx) const override;
private:
    PostFeedWsManager& ws_manager_;
};

} // namespace social_net_service::post
