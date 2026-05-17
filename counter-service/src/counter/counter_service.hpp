#pragma once

#include <memory>

#include <userver/components/component_fwd.hpp>
#include <userver/storages/redis/client.hpp>
#include <userver/ugrpc/server/service_component_base.hpp>

#include <counter/v1/counter_service.usrv.pb.hpp>

namespace counter_service::counter
{

class CounterServiceImpl final : public ::counter::v1::CounterServiceBase
{
public:
    explicit CounterServiceImpl(
        std::shared_ptr<userver::storages::redis::Client> redis);

    GetUnreadCountResult GetUnreadCount(
        CallContext& context,
        ::counter::v1::GetUnreadCountRequest&& request) override;

    ResetDialogCounterResult ResetDialogCounter(
        CallContext& context,
        ::counter::v1::ResetDialogCounterRequest&& request) override;

private:
    std::shared_ptr<userver::storages::redis::Client> redis_client_;
};

class CounterServiceComponent final
    : public userver::ugrpc::server::ServiceComponentBase
{
public:
    static constexpr std::string_view kName = "counter-grpc-service";

    CounterServiceComponent(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context);

private:
    CounterServiceImpl service_;
};

} // namespace counter_service::counter
