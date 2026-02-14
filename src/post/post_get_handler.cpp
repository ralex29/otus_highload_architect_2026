#include "post_get_handler.hpp"

#include "db/data_base.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

namespace social_net_service::post
{
    PostGetHandler::PostGetHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerBase(config, component_context)
        , pg_cluster_(
              component_context.FindComponent<userver::components::Postgres>(DataBase::Name)
                               .GetCluster())
    {
    }

    std::string PostGetHandler::HandleRequestThrow(
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
            "SELECT post_id, text, author_user_id "
            "FROM social_net_schema.posts "
            "WHERE post_id = $1",
            post_id
        );

        if (result.IsEmpty())
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return {};
        }

        auto json_result = userver::formats::json::MakeObject(
            "id", boost::uuids::to_string(result[0]["post_id"].As<boost::uuids::uuid>()),
            "text", result[0]["text"].As<std::string>(),
            "author_user_id", boost::uuids::to_string(result[0]["author_user_id"].As<boost::uuids::uuid>())
        );

        return userver::formats::json::ToString(json_result);
    }
} // social_net_service::post