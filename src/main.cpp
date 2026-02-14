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
#include <userver/storages/redis/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>

#include <userver/utils/daemon_run.hpp>

#include <user/user_get_handler.hpp>

#include "auth/login.hpp"
#include "auth/auth_bearer.hpp"
#include "auth/user_info_cache.hpp"
#include "user/user_search_handler.hpp"
#include "user/user_register_handler.hpp"
#include "friend/friend_add_handler.hpp"
#include "friend/friend_delete_handler.hpp"
#include "post/post_create_handler.hpp"
#include "post/post_update_handler.hpp"
#include "post/post_delete_handler.hpp"
#include "post/post_get_handler.hpp"
#include "post/post_feed_handler.hpp"
#include "post/post_feed_cache.hpp"
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
        .Append<userver::components::Secdist>()
        .Append<userver::components::DefaultSecdistProvider>()
        .Append<userver::components::Redis>("key-value-database")
        .Append<social_net_service::AuthCache>()
        .Append<userver::components::Postgres>(social_net_service::DataBase::Name)
        .Append<social_net_service::post::PostFeedCache>()
        .Append<social_net_service::user::UserRegisterHandler>()
        .Append<social_net_service::user::UserGetHandler>()
        .Append<social_net_service::user::UserSearchHandler>()
        .Append<social_net_service::auth::Login>()
        .Append<social_net_service::friend_ns::FriendAddHandler>()
        .Append<social_net_service::friend_ns::FriendDeleteHandler>()
        .Append<social_net_service::post::PostCreateHandler>()
        .Append<social_net_service::post::PostUpdateHandler>()
        .Append<social_net_service::post::PostDeleteHandler>()
        .Append<social_net_service::post::PostGetHandler>()
        .Append<social_net_service::post::PostFeedHandler>();

    return userver::utils::DaemonMain(argc, argv, component_list);
}
