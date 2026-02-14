#pragma once

#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

namespace social_net_service::post
{
    class PostFeedCache;

    class PostFeedHandler final : public userver::server::handlers::HttpHandlerBase
    {
    public:
        static constexpr std::string_view kName = "handler-post-feed";

        PostFeedHandler(const userver::components::ComponentConfig&, const userver::components::ComponentContext&);

        std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
                                       userver::server::request::RequestContext&) const override;

    private:
        PostFeedCache& feed_cache_;
    };
} // social_net_service::post
