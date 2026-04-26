#include "dialog_service.hpp"

#include <grpcpp/support/status.h>

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/storages/redis/reply.hpp>
#include <userver/ugrpc/server/exceptions.hpp>

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
    std::shared_ptr<userver::storages::redis::Client> redis)
    : redis_client_(std::move(redis))
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
              .GetClient("dialog-redis"))
{
    RegisterService(service_);
}

} // namespace chat_service::dialog
