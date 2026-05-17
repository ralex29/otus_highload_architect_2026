#include <userver/clients/dns/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/congestion_control/component.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/urabbitmq/component.hpp>
#include <userver/ugrpc/server/component_list.hpp>
#include <userver/utils/daemon_run.hpp>

#include "counter/counter_service.hpp"
#include "counter/counter_consumer.hpp"
#include "counter/counter_reconciler.hpp"

int main(int argc, char* argv[])
{
    auto component_list =
        userver::components::MinimalServerComponentList()
        .Append<userver::server::handlers::Ping>()
        .Append<userver::components::TestsuiteSupport>()
        .Append<userver::server::handlers::TestsControl>()
        .Append<userver::clients::dns::Component>()
        .Append<userver::congestion_control::Component>()
        .Append<userver::components::Secdist>()
        .Append<userver::components::DefaultSecdistProvider>()
        .Append<userver::components::Redis>("key-value-database")
        .Append<userver::components::RabbitMQ>("rabbitmq-driver")
        .AppendComponentList(userver::ugrpc::server::DefaultComponentList())
        .Append<counter_service::counter::CounterServiceComponent>()
        .Append<counter_service::counter::CounterConsumer>()
        .Append<counter_service::counter::CounterReconciler>();

    return userver::utils::DaemonMain(argc, argv, component_list);
}
