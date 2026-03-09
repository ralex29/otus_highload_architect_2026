#pragma once

#include <memory>

#include <userver/components/loggable_component_base.hpp>

namespace social_net_service::post {

/// Per-instance WebSocket fanout consumer.
/// On startup declares an auto-delete queue with a UUID name, binds it to the
/// "ws-fanout" fanout exchange, and starts consuming. Each app replica gets its
/// own copy of every WS-notification message, so local ws_manager can route it
/// to whichever WebSocket connections are open on this instance.
class WsFanoutConsumer final
    : public userver::components::LoggableComponentBase {
public:
    static constexpr std::string_view kName = "ws-fanout-consumer";

    WsFanoutConsumer(const userver::components::ComponentConfig&,
                     const userver::components::ComponentContext&);
    ~WsFanoutConsumer() override;

    static userver::yaml_config::Schema GetStaticConfigSchema();

protected:
    void OnAllComponentsLoaded() override;
    void OnAllComponentsAreStopping() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace social_net_service::post
