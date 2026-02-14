#include "post_delete_handler.hpp"

#include "db/data_base.hpp"
#include "post_feed_cache.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>

namespace social_net_service::post
{
    PostDeleteHandler::PostDeleteHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerBase(config, component_context)
        , pg_cluster_(
              component_context.FindComponent<userver::components::Postgres>(DataBase::Name)
                               .GetCluster())
        , feed_cache_(
              component_context.FindComponent<PostFeedCache>())
    {
    }

    std::string PostDeleteHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext&) const
    {
        boost::uuids::string_generator generator;
        boost::uuids::uuid post_id;
        try
        {
            post_id = generator(request.GetPathArg(0));
        }
        catch (const std::runtime_error&)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return {};
        }

        const auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "DELETE FROM social_net_schema.posts "
            "WHERE post_id = $1 "
            "RETURNING author_user_id",
            post_id
        );

        if (!result.IsEmpty())
        {
            const auto author_id = result.AsSingleRow<boost::uuids::uuid>();
            feed_cache_.InvalidateFeedsOfFollowersOf(author_id);
        }

        request.SetResponseStatus(userver::server::http::HttpStatus::kOk);
        return {};
    }
} // social_net_service::post
