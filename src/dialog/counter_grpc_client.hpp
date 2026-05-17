#pragma once

#include <userver/ugrpc/client/simple_client_component.hpp>

#include <counter/v1/counter_client.usrv.pb.hpp>

namespace social_net_service::dialog
{
    using CounterGrpcClientComponent =
        userver::ugrpc::client::SimpleClientComponent<counter::v1::CounterServiceClient>;

} // namespace social_net_service::dialog
