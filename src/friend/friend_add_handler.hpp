#pragma once

#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace social_net_service::post { class PostFeedCache; }

namespace social_net_service::friend_ns
{
    class FriendAddHandler final : public userver::server::handlers::HttpHandlerBase
    {
    public:
        static constexpr std::string_view kName = "handler-friend-add";

        FriendAddHandler(const userver::components::ComponentConfig&, const userver::components::ComponentContext&);

        std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
                                       userver::server::request::RequestContext&) const override;

    private:
        userver::storages::postgres::ClusterPtr pg_cluster_;
        post::PostFeedCache& feed_cache_;
    };
} // social_net_service::friend_ns
