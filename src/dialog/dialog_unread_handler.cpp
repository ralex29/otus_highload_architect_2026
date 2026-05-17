#include "dialog_unread_handler.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/ugrpc/client/exceptions.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <counter/v1/counter_client.usrv.pb.hpp>

namespace social_net_service::dialog
{

DialogUnreadHandler::DialogUnreadHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context)
    , counter_client_(
          context
              .FindComponent<CounterGrpcClientComponent>("counter-grpc-client")
              .GetClient())
{
}

std::string DialogUnreadHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& /*request*/,
    userver::server::request::RequestContext& request_context) const
{
    const auto user_id = request_context.GetData<boost::uuids::uuid>("user_id");

    counter::v1::GetUnreadCountRequest req;
    userver::ugrpc::client::CallOptions opts;
    opts.AddMetadata("x-user-id", boost::uuids::to_string(user_id));

    counter::v1::GetUnreadCountResponse resp;
    try
    {
        resp = counter_client_.GetUnreadCount(req, std::move(opts));
    }
    catch (const userver::ugrpc::client::RpcError&)
    {
        resp.set_total(0);
    }

    userver::formats::json::ValueBuilder result;
    result["unread_count"] = resp.total();
    return userver::formats::json::ToString(result.ExtractValue());
}

} // namespace social_net_service::dialog
