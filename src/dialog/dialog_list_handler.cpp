#include "dialog_list_handler.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/storages/redis/reply.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

namespace social_net_service::dialog
{
    DialogListHandler::DialogListHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerBase(config, component_context)
        , redis_client_(component_context
              .FindComponent<userver::components::Redis>("key-value-database")
              .GetClient("feed-redis"))
    {
    }

    std::string DialogListHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& request_context) const
    {
        const auto user_id = request_context.GetData<boost::uuids::uuid>("user_id");

        boost::uuids::string_generator str_gen;
        boost::uuids::uuid other_user_id;
        try
        {
            other_user_id = str_gen(request.GetPathArg("user_id"));
        }
        catch (const std::runtime_error&)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return {};
        }

        const auto reply = redis_client_->GenericCommand<userver::storages::redis::ReplyData>(
            "FCALL",
            {
                "dialog_list",
                "0",
                boost::uuids::to_string(user_id),
                boost::uuids::to_string(other_user_id)
            },
            0,
            {}
        ).Get();

        userver::formats::json::ValueBuilder json_array(userver::formats::json::Type::kArray);
        for (const auto& item : reply.GetArray())
        {
            json_array.PushBack(userver::formats::json::FromString(item.GetString()));
        }

        return userver::formats::json::ToString(json_array.ExtractValue());
    }
} // social_net_service::dialog
