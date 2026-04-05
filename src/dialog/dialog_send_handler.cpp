#include "dialog_send_handler.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/storages/redis/reply.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
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
        , redis_client_(component_context
              .FindComponent<userver::components::Redis>("key-value-database")
              .GetClient("feed-redis"))
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

        boost::uuids::string_generator str_gen;
        boost::uuids::uuid to_user_id;
        try
        {
            to_user_id = str_gen(request.GetPathArg("user_id"));
        }
        catch (const std::runtime_error&)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return {};
        }

        const auto text = request_json[kTextFieldName].As<std::string>();
        const auto msg_id = boost::uuids::to_string(boost::uuids::random_generator()());

        redis_client_->GenericCommand<userver::storages::redis::ReplyData>(
            "FCALL",
            {
                "dialog_send",
                "0",
                boost::uuids::to_string(from_user_id),
                boost::uuids::to_string(to_user_id),
                text,
                msg_id
            },
            0,
            {}
        ).Get();

        request.SetResponseStatus(userver::server::http::HttpStatus::kOk);
        return {};
    }
} // social_net_service::dialog
