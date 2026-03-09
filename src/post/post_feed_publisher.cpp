#include "post_feed_publisher.hpp"

#include <chrono>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/urabbitmq/component.hpp>
#include <userver/urabbitmq/typedefs.hpp>
#include <userver/engine/deadline.hpp>
#include <userver/utils/flags.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include <boost/uuid/uuid_io.hpp>

namespace social_net_service::post
{
    PostFeedPublisher::PostFeedPublisher(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : LoggableComponentBase(config, context)
        , client_(context.FindComponent<userver::components::RabbitMQ>(
              config["rabbit_name"].As<std::string>()).GetClient())
        , exchange_(config["exchange"].As<std::string>())
    {
        auto deadline = userver::engine::Deadline::FromDuration(std::chrono::seconds{10});

        client_->DeclareExchange(
            exchange_,
            userver::urabbitmq::Exchange::Type::kTopic,
            userver::utils::Flags<userver::urabbitmq::Exchange::Flags>{
                userver::urabbitmq::Exchange::Flags::kDurable},
            deadline);

        client_->DeclareQueue(
            userver::urabbitmq::Queue{"feed-materialization"},
            userver::utils::Flags<userver::urabbitmq::Queue::Flags>{
                userver::urabbitmq::Queue::Flags::kDurable},
            deadline);

        client_->BindQueue(
            exchange_,
            userver::urabbitmq::Queue{"feed-materialization"},
            "post.created.#",
            deadline);
    }

    userver::yaml_config::Schema PostFeedPublisher::GetStaticConfigSchema()
    {
        return userver::yaml_config::MergeSchemas<LoggableComponentBase>(R"(
type: object
description: RabbitMQ post feed publisher
additionalProperties: false
properties:
    rabbit_name:
        type: string
        description: RabbitMQ component name
    exchange:
        type: string
        description: Exchange name
)");
    }

    void PostFeedPublisher::Publish(
        const boost::uuids::uuid& author_id,
        const boost::uuids::uuid& post_id,
        const std::string& text)
    {
        userver::formats::json::ValueBuilder builder;
        builder["postId"] = boost::uuids::to_string(post_id);
        builder["postText"] = text;
        builder["author_user_id"] = boost::uuids::to_string(author_id);
        const auto msg = userver::formats::json::ToString(builder.ExtractValue());

        const auto routing_key = "post.created." + boost::uuids::to_string(author_id);

        client_->PublishReliable(
            exchange_,
            routing_key,
            msg,
            userver::urabbitmq::MessageType::kPersistent,
            userver::engine::Deadline::FromDuration(std::chrono::seconds{5}));
    }
} // namespace social_net_service::post
