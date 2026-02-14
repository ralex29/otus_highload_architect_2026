#include "post_update_handler.hpp"

#include "db/data_base.hpp"
#include "post_feed_cache.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/value.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>

namespace
{
    constexpr std::string_view kIdFieldName = "id";
    constexpr std::string_view kTextFieldName = "text";

    constexpr std::string_view kErrorMembersNotSet = R"(
  {
    "error": "Expected body has `id` and `text` fields"
  }
)";

    bool IsCorrectRequest(const userver::formats::json::Value& request_json)
    {
        return request_json.HasMember(kIdFieldName)
            && request_json.HasMember(kTextFieldName);
    }
} // namespace

namespace social_net_service::post
{
    PostUpdateHandler::PostUpdateHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerJsonBase(config, component_context)
        , pg_cluster_(
              component_context.FindComponent<userver::components::Postgres>(DataBase::Name)
                               .GetCluster())
        , feed_cache_(
              component_context.FindComponent<PostFeedCache>())
    {
    }

    userver::formats::json::Value PostUpdateHandler::HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_json,
        userver::server::request::RequestContext&) const
    {
        if (!IsCorrectRequest(request_json))
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return userver::formats::json::FromString(kErrorMembersNotSet);
        }

        boost::uuids::string_generator generator;
        boost::uuids::uuid post_id;
        try
        {
            post_id = generator(request_json[kIdFieldName].As<std::string>());
        }
        catch (const std::runtime_error&)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return userver::formats::json::MakeObject("error", "Invalid UUID format");
        }

        const auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "UPDATE social_net_schema.posts "
            "SET text = $2 "
            "WHERE post_id = $1 "
            "RETURNING author_user_id",
            post_id,
            request_json[kTextFieldName].As<std::string>()
        );

        if (!result.IsEmpty())
        {
            const auto author_id = result.AsSingleRow<boost::uuids::uuid>();
            feed_cache_.InvalidateFeedsOfFollowersOf(author_id);
        }

        request.SetResponseStatus(userver::server::http::HttpStatus::kOk);
        return userver::formats::json::MakeObject();
    }
} // social_net_service::post
