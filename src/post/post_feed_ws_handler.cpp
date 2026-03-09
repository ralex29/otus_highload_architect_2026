#include "post_feed_ws_handler.hpp"
#include "post_feed_ws_manager.hpp"

#include <atomic>
#include <chrono>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/io/exception.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/logging/log.hpp>
#include <userver/websocket/message.hpp>

namespace social_net_service::post {

PostFeedWsHandler::PostFeedWsHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& component_context)
    : WebsocketHandlerBase(config, component_context)
    , ws_manager_(component_context.FindComponent<PostFeedWsManager>())
{
}

void PostFeedWsHandler::Handle(
    userver::websocket::WebSocketConnection& ws,
    userver::server::request::RequestContext& ctx) const
{
    const auto user_id = ctx.GetData<boost::uuids::uuid>("user_id");
    auto session = ws_manager_.Subscribe(user_id);

    std::atomic<bool> disconnected{false};

    // Background task: drain incoming frames and detect client close
    auto recv_task = userver::engine::AsyncNoSpan(
        [&ws, &disconnected]() {
            userver::websocket::Message msg;
            try {
                while (!userver::engine::current_task::ShouldCancel()) {
                    ws.Recv(msg);
                    if (msg.close_status.has_value()) {
                        disconnected.store(true, std::memory_order_release);
                        return;
                    }
                }
            } catch (const userver::engine::io::IoException&) {
                disconnected.store(true, std::memory_order_release);
            }
        });

    // Main loop: pop from queue and send to WebSocket client
    const auto deadline_duration = std::chrono::seconds{1};
    while (!disconnected.load(std::memory_order_acquire) &&
           !userver::engine::current_task::ShouldCancel()) {
        std::string msg;
        const bool got = session->consumer.Pop(
            msg, userver::engine::Deadline::FromDuration(deadline_duration));
        if (!got) continue;
        try {
            ws.SendText(msg);
        } catch (const userver::engine::io::IoException& e) {
            LOG_INFO() << "WS feed send error: " << e.what();
            break;
        }
    }

    recv_task.RequestCancel();
    recv_task.Wait();
    ws_manager_.Unsubscribe(user_id, session);
    ws.Close(userver::websocket::CloseStatus::kGoingAway);
}

} // namespace social_net_service::post
