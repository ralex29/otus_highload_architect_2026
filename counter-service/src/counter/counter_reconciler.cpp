#include "counter_reconciler.hpp"

#include <chrono>

#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/storages/redis/reply.hpp>
#include <userver/utils/periodic_task.hpp>

namespace counter_service::counter
{

CounterReconciler::CounterReconciler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : LoggableComponentBase(config, context)
    , redis_client_(
          context
              .FindComponent<userver::components::Redis>("key-value-database")
              .GetClient("counter-redis"))
{
    task_.Start(
        "counter-reconciler",
        userver::utils::PeriodicTask::Settings{std::chrono::seconds{60}},
        [this] { RunReconciliation(); });
}

CounterReconciler::~CounterReconciler()
{
    task_.Stop();
}

void CounterReconciler::RunReconciliation()
{
    // Scan all counter keys and reconcile each user's total.
    std::string cursor = "0";
    do
    {
        auto reply =
            redis_client_->GenericCommand<userver::storages::redis::ReplyData>(
                "SCAN",
                {cursor, "MATCH", "counters:*", "COUNT", "100"},
                0,
                {}).Get();

        const auto& arr = reply.GetArray();
        cursor = arr[0].GetString();
        const auto& keys = arr[1].GetArray();

        for (const auto& key_entry : keys)
        {
            const std::string key = key_entry.GetString();
            // Extract user_id from "counters:{user_id}"
            const std::string user_id = key.substr(std::string{"counters:"}.size());
            try
            {
                redis_client_->GenericCommand<userver::storages::redis::ReplyData>(
                    "FCALL",
                    {"counter_reconcile_user", "0", user_id},
                    0,
                    {}).Get();
            }
            catch (const std::exception& e)
            {
                LOG_WARNING() << "reconciler: failed for user " << user_id
                              << ": " << e.what();
            }
        }
    } while (cursor != "0");
}

} // namespace counter_service::counter
