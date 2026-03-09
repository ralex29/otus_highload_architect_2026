#include "post_feed_consumer.hpp"
#include "post_feed_cache.hpp"

#include <chrono>

#include <boost/uuid/string_generator.hpp>

#include <userver/components/component_context.hpp>
#include <userver/engine/deadline.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/urabbitmq/component.hpp>
#include <userver/urabbitmq/typedefs.hpp>

namespace social_net_service::post
{
    PostFeedConsumer::PostFeedConsumer(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : ConsumerComponentBase(config, context)
        , feed_cache_(context.FindComponent<PostFeedCache>())
        , client_(context.FindComponent<userver::components::RabbitMQ>(
              "rabbitmq-driver").GetClient())
    {
    }

    void PostFeedConsumer::Process(userver::urabbitmq::ConsumedMessage msg)
    {
        const auto json = userver::formats::json::FromString(msg.message);

        boost::uuids::string_generator gen;
        const auto post_id   = gen(json["postId"].As<std::string>());
        const auto author_id = gen(json["author_user_id"].As<std::string>());
        const auto text      = json["postText"].As<std::string>();

        // 1. Materialise Redis feed cache for all followers
        feed_cache_.OnPostCreated(author_id, post_id, text);

        // 2. Fan out WS notification to every app instance.
        //    Each instance has its own auto-delete queue bound to ws-fanout,
        //    so the message is delivered to all of them simultaneously.
        client_->Publish(
            userver::urabbitmq::Exchange{"ws-fanout"},
            "",         // routing key is ignored for fanout exchanges
            msg.message,
            userver::engine::Deadline::FromDuration(std::chrono::seconds{5}));
    }
} // namespace social_net_service::post
