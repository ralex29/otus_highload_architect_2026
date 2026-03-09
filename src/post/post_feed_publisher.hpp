#pragma once

#include <string>
#include <boost/uuid/uuid.hpp>
#include <userver/components/loggable_component_base.hpp>
#include <userver/urabbitmq/client.hpp>
#include <userver/urabbitmq/typedefs.hpp>

namespace social_net_service::post
{
    class PostFeedPublisher final : public userver::components::LoggableComponentBase
    {
    public:
        static constexpr std::string_view kName = "post-feed-publisher";

        PostFeedPublisher(const userver::components::ComponentConfig&,
                          const userver::components::ComponentContext&);

        static userver::yaml_config::Schema GetStaticConfigSchema();

        void Publish(const boost::uuids::uuid& author_id,
                     const boost::uuids::uuid& post_id,
                     const std::string& text);

    private:
        std::shared_ptr<userver::urabbitmq::Client> client_;
        userver::urabbitmq::Exchange exchange_;
    };
} // namespace social_net_service::post
