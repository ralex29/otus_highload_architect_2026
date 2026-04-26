#pragma once

#include <userver/ugrpc/client/simple_client_component.hpp>

#include <chat/v1/dialog_client.usrv.pb.hpp>

namespace social_net_service::dialog
{
    // gRPC client component for the chat-service.
    // Registered in the component list as "dialog-grpc-client".
    using DialogGrpcClientComponent =
        userver::ugrpc::client::SimpleClientComponent<chat::v1::DialogServiceClient>;

} // namespace social_net_service::dialog
