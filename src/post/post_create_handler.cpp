#include "post_create_handler.hpp"

#include "db/data_base.hpp"
#include "post_feed_cache.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/value.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace
{
    constexpr std::string_view kTextFieldName = "text";

    constexpr std::string_view kErrorMembersNotSet = R"(
  {
    "error": "Expected body has `text` field"
  }
)";

    bool IsCorrectRequest(const userver::formats::json::Value& request_json)
    {
        return request_json.HasMember(kTextFieldName);
    }
} // namespace

namespace social_net_service::post
{
    PostCreateHandler::PostCreateHandler(
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

    userver::formats::json::Value PostCreateHandler::HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_json,
        userver::server::request::RequestContext& request_context) const
    {
        if (!IsCorrectRequest(request_json))
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return userver::formats::json::FromString(kErrorMembersNotSet);
        }

        const auto author_id = request_context.GetData<boost::uuids::uuid>("user_id");
        const auto text = request_json[kTextFieldName].As<std::string>();

        const auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "INSERT INTO social_net_schema.posts (author_user_id, text) "
            "VALUES ($1, $2) "
            "RETURNING post_id",
            author_id,
            text
        );

        const auto post_id = result.AsSingleRow<boost::uuids::uuid>();

        feed_cache_.OnPostCreated(author_id, post_id, text);

        return userver::formats::json::MakeObject("id", boost::uuids::to_string(post_id));
    }
} // social_net_service::post
