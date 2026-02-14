#include "friend_delete_handler.hpp"

#include "db/data_base.hpp"
#include "post/post_feed_cache.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>

namespace social_net_service::friend_ns
{
    FriendDeleteHandler::FriendDeleteHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerBase(config, component_context)
        , pg_cluster_(
              component_context.FindComponent<userver::components::Postgres>(DataBase::Name)
                               .GetCluster())
        , feed_cache_(
              component_context.FindComponent<post::PostFeedCache>())
    {
    }

    std::string FriendDeleteHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& request_context) const
    {
        const auto user_id = request_context.GetData<boost::uuids::uuid>("user_id");

        boost::uuids::string_generator generator;
        boost::uuids::uuid friend_id;
        try
        {
            friend_id = generator(request.GetPathArg(0));
        }
        catch (const std::runtime_error&)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return {};
        }

        pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "DELETE FROM social_net_schema.friendships "
            "WHERE user_id = $1 AND friend_id = $2",
            user_id,
            friend_id
        );

        feed_cache_.InvalidateFeed(user_id);

        request.SetResponseStatus(userver::server::http::HttpStatus::kOk);
        return {};
    }
} // social_net_service::friend_ns
