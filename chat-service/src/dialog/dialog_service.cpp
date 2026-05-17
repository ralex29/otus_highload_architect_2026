#include "dialog_service.hpp"

#include <chrono>

#include <grpcpp/support/status.h>

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/engine/deadline.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/storages/redis/reply.hpp>
#include <userver/ugrpc/server/exceptions.hpp>
#include <userver/urabbitmq/component.hpp>
#include <userver/urabbitmq/typedefs.hpp>
#include <userver/utils/flags.hpp>

namespace chat_service::dialog
{

namespace
{

// Extracts the authenticated user ID injected by the monolith into gRPC metadata.
// The key must be lowercase — gRPC normalises metadata keys to lower case.
std::string ExtractUserId(userver::ugrpc::server::CallContextBase& context)
{
    const auto& metadata = context.GetServerContext().client_metadata();
    const auto it = metadata.find("x-user-id");
    if (it == metadata.end())
    {
        throw userver::ugrpc::server::ErrorWithStatus(
            grpc::StatusCode::UNAUTHENTICATED, "missing x-user-id metadata");
    }
    return std::string{it->second.data(), it->second.size()};
}

std::string GetString(const userver::formats::json::Value& v)
{
    return v.As<std::string>();
}

} // namespace

DialogServiceImpl::DialogServiceImpl(
    std::shared_ptr<userver::storages::redis::Client> redis,
    std::shared_ptr<userver::urabbitmq::Client> rabbit)
    : redis_client_(std::move(redis))
    , rabbit_client_(std::move(rabbit))
{
}

DialogServiceImpl::SendMessageResult DialogServiceImpl::SendMessage(
    CallContext& context,
    chat::v1::SendMessageRequest&& request)
{
    const std::string from_user_id = ExtractUserId(context);
    const std::string& to_user_id = request.to_user_id();
    const std::string& text = request.text();
    const std::string msg_id =
        boost::uuids::to_string(boost::uuids::random_generator()());

    redis_client_->GenericCommand<userver::storages::redis::ReplyData>(
        "FCALL",
        {"dialog_send", "0", from_user_id, to_user_id, text, msg_id},
        0,
        {}
    ).Get();

    // SAGA step 2: publish message.sent event so counter-service increments
    // the recipient's unread counter. PublishReliable ensures at-least-once delivery.
    userver::formats::json::ValueBuilder evt;
    evt["from_user_id"] = from_user_id;
    evt["to_user_id"]   = to_user_id;

    rabbit_client_->PublishReliable(
        userver::urabbitmq::Exchange{"messages-exchange"},
        "message.sent." + to_user_id,
        userver::formats::json::ToString(evt.ExtractValue()),
        userver::urabbitmq::MessageType::kPersistent,
        userver::engine::Deadline::FromDuration(std::chrono::seconds{5}));

    return chat::v1::SendMessageResponse{};
}

DialogServiceImpl::ListMessagesResult DialogServiceImpl::ListMessages(
    CallContext& context,
    chat::v1::ListMessagesRequest&& request)
{
    const std::string user_id = ExtractUserId(context);
    const std::string& other_user_id = request.other_user_id();

    const auto reply =
        redis_client_->GenericCommand<userver::storages::redis::ReplyData>(
            "FCALL",
            {"dialog_list", "0", user_id, other_user_id},
            0,
            {}
        ).Get();

    chat::v1::ListMessagesResponse response;
    for (const auto& item : reply.GetArray())
    {
        const auto json = userver::formats::json::FromString(item.GetString());
        auto* msg = response.add_messages();
        msg->set_from_user_id(GetString(json["from"]));
        msg->set_to_user_id(GetString(json["to"]));
        msg->set_text(GetString(json["text"]));
    }
    return response;
}

DialogServiceComponent::DialogServiceComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ServiceComponentBase(config, context)
    , service_(
          context
              .FindComponent<userver::components::Redis>("key-value-database")
              .GetClient("dialog-redis"),
          [&] {
              auto client =
                  context.FindComponent<userver::components::RabbitMQ>("rabbitmq-driver")
                         .GetClient();
              // Declare the exchange idempotently so any service start order works.
              client->DeclareExchange(
                  userver::urabbitmq::Exchange{"messages-exchange"},
                  userver::urabbitmq::Exchange::Type::kTopic,
                  userver::utils::Flags<userver::urabbitmq::Exchange::Flags>{
                      userver::urabbitmq::Exchange::Flags::kDurable},
                  userver::engine::Deadline::FromDuration(std::chrono::seconds{10}));
              return client;
          }())
{
    RegisterService(service_);
}

} // namespace chat_service::dialog
