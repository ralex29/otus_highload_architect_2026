#pragma once

#include <memory>

#include <userver/urabbitmq/client.hpp>
#include <userver/urabbitmq/consumer_component_base.hpp>

namespace social_net_service::post
{
    class PostFeedCache;

    class PostFeedConsumer final : public userver::urabbitmq::ConsumerComponentBase
    {
    public:
        static constexpr std::string_view kName = "post-feed-consumer";

        PostFeedConsumer(const userver::components::ComponentConfig&,
                         const userver::components::ComponentContext&);

    protected:
        void Process(userver::urabbitmq::ConsumedMessage msg) override;

    private:
        PostFeedCache& feed_cache_;
        // Publishes the same message to ws-fanout so every app instance
        // gets a copy and can notify its local WebSocket sessions.
        std::shared_ptr<userver::urabbitmq::Client> client_;
    };
} // namespace social_net_service::post
