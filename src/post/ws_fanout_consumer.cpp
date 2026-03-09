#include "ws_fanout_consumer.hpp"
#include "post_feed_ws_manager.hpp"

#include <chrono>
#include <memory>
#include <string>

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/engine/deadline.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/urabbitmq/client.hpp>
#include <userver/urabbitmq/component.hpp>
#include <userver/urabbitmq/consumer_base.hpp>
#include <userver/urabbitmq/consumer_settings.hpp>
#include <userver/urabbitmq/typedefs.hpp>
#include <userver/utils/flags.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace social_net_service::post {

struct WsFanoutConsumer::Impl : userver::urabbitmq::ConsumerBase {
    PostFeedWsManager& ws_manager_;

    Impl(std::shared_ptr<userver::urabbitmq::Client> client,
         const std::string& queue_name,
         PostFeedWsManager& ws_manager)
        : ConsumerBase(
              std::move(client),
              userver::urabbitmq::ConsumerSettings{
                  userver::urabbitmq::Queue{queue_name}, 1})
        , ws_manager_(ws_manager)
    {}

    void Process(userver::urabbitmq::ConsumedMessage msg) override {
        const auto json = userver::formats::json::FromString(msg.message);
        boost::uuids::string_generator gen;
        const auto author_id = gen(json["author_user_id"].As<std::string>());
        const auto post_id   = gen(json["postId"].As<std::string>());
        const auto text      = json["postText"].As<std::string>();
        ws_manager_.NotifyFollowers(author_id, post_id, text);
    }
};

// ------------------------------------------------------------
WsFanoutConsumer::WsFanoutConsumer(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : LoggableComponentBase(config, context)
{
    auto client =
        context.FindComponent<userver::components::RabbitMQ>("rabbitmq-driver")
               .GetClient();
    auto& ws_manager = context.FindComponent<PostFeedWsManager>();

    // Unique queue name per instance — auto-delete ensures cleanup on shutdown
    const std::string queue_name =
        "ws-fanout-" +
        boost::uuids::to_string(boost::uuids::random_generator()());

    const auto deadline =
        userver::engine::Deadline::FromDuration(std::chrono::seconds{10});

    // Declare the fanout exchange (idempotent across instances)
    client->DeclareExchange(
        userver::urabbitmq::Exchange{"ws-fanout"},
        userver::urabbitmq::Exchange::Type::kFanOut,
        deadline);

    // Declare this instance's auto-delete queue (not exclusive — ConsumerBase
    // connects on a separate AMQP connection from the declaring client)
    client->DeclareQueue(
        userver::urabbitmq::Queue{queue_name},
        userver::utils::Flags<userver::urabbitmq::Queue::Flags>{
            userver::urabbitmq::Queue::Flags::kAutoDelete},
        deadline);

    // Bind: all messages published to ws-fanout reach this queue
    client->BindQueue(
        userver::urabbitmq::Exchange{"ws-fanout"},
        userver::urabbitmq::Queue{queue_name},
        "", // routing key is ignored for fanout exchanges
        deadline);

    impl_ = std::make_unique<Impl>(std::move(client), queue_name, ws_manager);
}

WsFanoutConsumer::~WsFanoutConsumer() = default;

userver::yaml_config::Schema WsFanoutConsumer::GetStaticConfigSchema()
{
    return userver::yaml_config::MergeSchemas<LoggableComponentBase>(R"(
type: object
description: Per-instance WebSocket fanout consumer
additionalProperties: false
properties: {}
)");
}

void WsFanoutConsumer::OnAllComponentsLoaded()  { impl_->Start(); }
void WsFanoutConsumer::OnAllComponentsAreStopping() { impl_->Stop(); }

} // namespace social_net_service::post
