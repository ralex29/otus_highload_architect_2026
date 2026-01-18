#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component_list.hpp>
#include <userver/components/component.hpp>
#include <userver/components/component_list.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/congestion_control/component.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>

#include <userver/storages/postgres/component.hpp>

#include <userver/utils/daemon_run.hpp>

#include <user/user_get_handler.hpp>

#include "auth/login.hpp"
#include "auth/auth_bearer.hpp"
#include "auth/user_info_cache.hpp"
#include "user/user_register_handler.hpp"
#include "db/data_base.hpp"


int main(int argc, char* argv[])
{
    userver::server::handlers::auth::RegisterAuthCheckerFactory<social_net_service::CheckerFactory>();
    auto component_list =
        userver::components::MinimalServerComponentList()
        .Append<userver::server::handlers::Ping>()
        .Append<userver::components::TestsuiteSupport>()
        .AppendComponentList(userver::clients::http::ComponentList())
        .Append<userver::clients::dns::Component>()
        .Append<userver::server::handlers::TestsControl>()
        .Append<userver::congestion_control::Component>()
        .Append<social_net_service::AuthCache>()
        .Append<userver::components::Postgres>(social_net_service::DataBase::Name)
        .Append<social_net_service::user::UserRegisterHandler>()
        .Append<social_net_service::user::UserGetHandler>()
        .Append<social_net_service::auth::Login>();

    return userver::utils::DaemonMain(argc, argv, component_list);
}
