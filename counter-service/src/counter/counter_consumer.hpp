#pragma once

#include <memory>

#include <userver/storages/redis/client.hpp>
#include <userver/urabbitmq/client.hpp>
#include <userver/urabbitmq/consumer_component_base.hpp>

namespace counter_service::counter
{

class CounterConsumer final : public userver::urabbitmq::ConsumerComponentBase
{
public:
    static constexpr std::string_view kName = "counter-consumer";

    CounterConsumer(const userver::components::ComponentConfig&,
                    const userver::components::ComponentContext&);

protected:
    void Process(userver::urabbitmq::ConsumedMessage msg) override;

private:
    std::shared_ptr<userver::storages::redis::Client> redis_client_;
};

} // namespace counter_service::counter
