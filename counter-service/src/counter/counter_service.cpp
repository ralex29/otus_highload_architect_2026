#include "counter_service.hpp"

#include <grpcpp/support/status.h>

#include <userver/components/component_context.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/ugrpc/server/exceptions.hpp>

namespace counter_service::counter
{

namespace
{

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

} // namespace

CounterServiceImpl::CounterServiceImpl(
    std::shared_ptr<userver::storages::redis::Client> redis)
    : redis_client_(std::move(redis))
{
}

CounterServiceImpl::GetUnreadCountResult CounterServiceImpl::GetUnreadCount(
    CallContext& context,
    ::counter::v1::GetUnreadCountRequest&& /*request*/)
{
    const std::string user_id = ExtractUserId(context);

    auto reply = redis_client_->Hget(
        "counters:" + user_id, "total", {}).Get();

    ::counter::v1::GetUnreadCountResponse response;
    response.set_total(reply ? std::stoll(*reply) : 0LL);
    return response;
}

CounterServiceImpl::ResetDialogCounterResult CounterServiceImpl::ResetDialogCounter(
    CallContext& context,
    ::counter::v1::ResetDialogCounterRequest&& request)
{
    const std::string user_id = ExtractUserId(context);
    const std::string& partner_id = request.partner_id();

    const auto reply =
        redis_client_->GenericCommand<userver::storages::redis::ReplyData>(
            "FCALL",
            {"counter_reset_dialog", "0", user_id, partner_id},
            0,
            {}).Get();

    ::counter::v1::ResetDialogCounterResponse response;
    response.set_dialog_count(reply.GetInt());
    return response;
}

CounterServiceComponent::CounterServiceComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ServiceComponentBase(config, context)
    , service_(
          context
              .FindComponent<userver::components::Redis>("key-value-database")
              .GetClient("counter-redis"))
{
    RegisterService(service_);
}

} // namespace counter_service::counter
