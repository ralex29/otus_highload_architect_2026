#include "dialog_send_handler.hpp"

#include "dialog_utils.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/component.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>

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

namespace social_net_service::dialog
{
    DialogSendHandler::DialogSendHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerJsonBase(config, component_context)
        , cluster_(component_context.FindComponent<userver::components::Postgres>("postgres-citus").GetCluster())
    {
    }

    userver::formats::json::Value DialogSendHandler::HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_json,
        userver::server::request::RequestContext& request_context) const
    {
        if (!IsCorrectRequest(request_json))
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return userver::formats::json::FromString(kErrorMembersNotSet);
        }

        const auto from_user_id = request_context.GetData<boost::uuids::uuid>("user_id");

        boost::uuids::string_generator generator;
        boost::uuids::uuid to_user_id;
        try
        {
            to_user_id = generator(request.GetPathArg("user_id"));
        }
        catch (const std::runtime_error&)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return {};
        }

        const auto text = request_json[kTextFieldName].As<std::string>();
        const int bucket = dialog::GetVirtualBucket(from_user_id, to_user_id);

        cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "INSERT INTO dialog_schema.messages (from_user_id, to_user_id, text, virtual_bucket) "
            "VALUES ($1, $2, $3, $4)",
            from_user_id,
            to_user_id,
            text,
            bucket
        );

        request.SetResponseStatus(userver::server::http::HttpStatus::kOk);
        return {};
    }
} // social_net_service::dialog
