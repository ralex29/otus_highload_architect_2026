#pragma once

#include <memory>

#include <userver/components/loggable_component_base.hpp>
#include <userver/storages/redis/client.hpp>
#include <userver/utils/periodic_task.hpp>

namespace counter_service::counter
{

// Periodically reconciles the 'total' field in each counter hash
// to equal the sum of all 'dialog:*' fields, correcting drift
// from partially-failed SAGA steps.
class CounterReconciler final : public userver::components::LoggableComponentBase
{
public:
    static constexpr std::string_view kName = "counter-reconciler";

    CounterReconciler(const userver::components::ComponentConfig&,
                      const userver::components::ComponentContext&);

    ~CounterReconciler() override;

private:
    void RunReconciliation();

    std::shared_ptr<userver::storages::redis::Client> redis_client_;
    userver::utils::PeriodicTask task_;
};

} // namespace counter_service::counter
