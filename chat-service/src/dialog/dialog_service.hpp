#pragma once

#include <memory>

#include <userver/components/component_fwd.hpp>
#include <userver/storages/redis/client.hpp>
#include <userver/ugrpc/server/service_component_base.hpp>

#include <chat/v1/dialog_service.usrv.pb.hpp>

namespace chat_service::dialog
{

class DialogServiceImpl final : public chat::v1::DialogServiceBase
{
public:
    explicit DialogServiceImpl(
        std::shared_ptr<userver::storages::redis::Client> redis);

    SendMessageResult SendMessage(
        CallContext& context,
        chat::v1::SendMessageRequest&& request) override;

    ListMessagesResult ListMessages(
        CallContext& context,
        chat::v1::ListMessagesRequest&& request) override;

private:
    std::shared_ptr<userver::storages::redis::Client> redis_client_;
};

class DialogServiceComponent final
    : public userver::ugrpc::server::ServiceComponentBase
{
public:
    static constexpr std::string_view kName = "dialog-grpc-service";

    DialogServiceComponent(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context);

private:
    DialogServiceImpl service_;
};

} // namespace chat_service::dialog
