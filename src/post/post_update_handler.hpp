#pragma once

#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace social_net_service::post
{
    class PostFeedCache;

    class PostUpdateHandler final : public userver::server::handlers::HttpHandlerJsonBase
    {
    public:
        static constexpr std::string_view kName = "handler-post-update";

        PostUpdateHandler(const userver::components::ComponentConfig&, const userver::components::ComponentContext&);

        userver::formats::json::Value HandleRequestJsonThrow(
            const userver::server::http::HttpRequest& request,
            const userver::formats::json::Value& request_json,
            userver::server::request::RequestContext& context) const override;

    private:
        userver::storages::postgres::ClusterPtr pg_cluster_;
        PostFeedCache& feed_cache_;
    };
} // social_net_service::post
