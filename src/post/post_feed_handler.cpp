#include "post_feed_handler.hpp"

#include "post_feed_cache.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace social_net_service::post
{
    PostFeedHandler::PostFeedHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerBase(config, component_context)
        , feed_cache_(
              component_context.FindComponent<PostFeedCache>())
    {
    }

    std::string PostFeedHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& request_context) const
    {
        int offset = 0;
        int limit = 10;
        try
        {
            if (request.HasArg("offset"))
                offset = std::stoi(request.GetArg("offset"));
            if (request.HasArg("limit"))
                limit = std::stoi(request.GetArg("limit"));
        }
        catch (const std::exception&)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return {};
        }

        const auto user_id = request_context.GetData<boost::uuids::uuid>("user_id");

        auto feed = feed_cache_.GetFeed(user_id, offset, limit);

        userver::formats::json::ValueBuilder json_array(userver::formats::json::Type::kArray);
        for (const auto& post : feed)
        {
            userver::formats::json::ValueBuilder json_post;
            json_post["id"] = boost::uuids::to_string(post.post_id);
            json_post["text"] = post.text;
            json_post["author_user_id"] = boost::uuids::to_string(post.author_user_id);
            json_array.PushBack(json_post.ExtractValue());
        }

        return userver::formats::json::ToString(json_array.ExtractValue());
    }
} // social_net_service::post
