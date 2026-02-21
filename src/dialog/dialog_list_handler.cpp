#include "dialog_list_handler.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/storages/postgres/component.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

namespace social_net_service::dialog
{
    DialogListHandler::DialogListHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerBase(config, component_context)
        , cluster_(component_context.FindComponent<userver::components::Postgres>("postgres-citus").GetCluster())
    {
    }

    std::string DialogListHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& request_context) const
    {
        const auto user_id = request_context.GetData<boost::uuids::uuid>("user_id");

        boost::uuids::string_generator generator;
        boost::uuids::uuid other_user_id;
        try
        {
            other_user_id = generator(request.GetPathArg("user_id"));
        }
        catch (const std::runtime_error&)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return {};
        }

        const auto result = cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT from_user_id, to_user_id, text "
            "FROM dialog_schema.messages "
            "WHERE (from_user_id = $1 AND to_user_id = $2) "
            "   OR (from_user_id = $2 AND to_user_id = $1) "
            "ORDER BY created_at ASC",
            user_id,
            other_user_id
        );

        userver::formats::json::ValueBuilder json_array(userver::formats::json::Type::kArray);
        for (const auto& row : result)
        {
            userver::formats::json::ValueBuilder json_msg;
            json_msg["from"] = boost::uuids::to_string(row["from_user_id"].As<boost::uuids::uuid>());
            json_msg["to"] = boost::uuids::to_string(row["to_user_id"].As<boost::uuids::uuid>());
            json_msg["text"] = row["text"].As<std::string>();
            json_array.PushBack(json_msg.ExtractValue());
        }

        return userver::formats::json::ToString(json_array.ExtractValue());
    }
} // social_net_service::dialog
