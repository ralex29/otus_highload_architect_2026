#include "counter_consumer.hpp"

#include <chrono>

#include <userver/components/component_context.hpp>
#include <userver/engine/deadline.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/urabbitmq/component.hpp>
#include <userver/urabbitmq/typedefs.hpp>
#include <userver/utils/flags.hpp>

namespace counter_service::counter
{

CounterConsumer::CounterConsumer(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ConsumerComponentBase(config, context)
    , redis_client_(
          context
              .FindComponent<userver::components::Redis>("key-value-database")
              .GetClient("counter-redis"))
{
    // Declare exchange and queue idempotently so counter-service can start
    // independently of chat-service.
    auto client =
        context.FindComponent<userver::components::RabbitMQ>("rabbitmq-driver")
               .GetClient();
    auto deadline = userver::engine::Deadline::FromDuration(std::chrono::seconds{10});

    client->DeclareExchange(
        userver::urabbitmq::Exchange{"messages-exchange"},
        userver::urabbitmq::Exchange::Type::kTopic,
        userver::utils::Flags<userver::urabbitmq::Exchange::Flags>{
            userver::urabbitmq::Exchange::Flags::kDurable},
        deadline);

    client->DeclareQueue(
        userver::urabbitmq::Queue{"message-counters"},
        userver::utils::Flags<userver::urabbitmq::Queue::Flags>{
            userver::urabbitmq::Queue::Flags::kDurable},
        deadline);

    client->BindQueue(
        userver::urabbitmq::Exchange{"messages-exchange"},
        userver::urabbitmq::Queue{"message-counters"},
        "message.sent.#",
        deadline);
}

void CounterConsumer::Process(userver::urabbitmq::ConsumedMessage msg)
{
    const auto json = userver::formats::json::FromString(msg.message);
    const auto to_user_id   = json["to_user_id"].As<std::string>();
    const auto from_user_id = json["from_user_id"].As<std::string>();

    const std::string hash_key    = "counters:" + to_user_id;
    const std::string dialog_field = "dialog:" + from_user_id;

    redis_client_->GenericCommand<userver::storages::redis::ReplyData>(
        "HINCRBY", {hash_key, "total", "1"}, 0, {}).Get();

    redis_client_->GenericCommand<userver::storages::redis::ReplyData>(
        "HINCRBY", {hash_key, dialog_field, "1"}, 0, {}).Get();
}

} // namespace counter_service::counter
