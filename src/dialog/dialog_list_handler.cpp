#include "dialog_list_handler.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/ugrpc/client/exceptions.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

#include <chat/v1/dialog_service.usrv.pb.hpp>

namespace social_net_service::dialog
{
    DialogListHandler::DialogListHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerBase(config, component_context)
        , grpc_client_(
              component_context
                  .FindComponent<DialogGrpcClientComponent>("dialog-grpc-client")
                  .GetClient())
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

        chat::v1::ListMessagesRequest grpc_req;
        grpc_req.set_other_user_id(boost::uuids::to_string(other_user_id));

        // Pass authenticated user_id to chat-service via gRPC metadata.
        // Span context (trace_id) is propagated automatically.
        userver::ugrpc::client::CallOptions opts;
        opts.AddMetadata("x-user-id", boost::uuids::to_string(user_id));

        chat::v1::ListMessagesResponse grpc_resp;
        try
        {
            grpc_resp = grpc_client_.ListMessages(grpc_req, std::move(opts));
        }
        catch (const userver::ugrpc::client::RpcError&)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
            return {};
        }

        userver::formats::json::ValueBuilder json_array(userver::formats::json::Type::kArray);
        for (const auto& msg : grpc_resp.messages())
        {
            userver::formats::json::ValueBuilder item;
            item["from"] = msg.from_user_id();
            item["to"]   = msg.to_user_id();
            item["text"] = msg.text();
            json_array.PushBack(std::move(item));
        }

        return userver::formats::json::ToString(json_array.ExtractValue());
    }
} // social_net_service::dialog
