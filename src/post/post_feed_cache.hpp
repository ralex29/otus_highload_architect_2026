#pragma once

#include <deque>
#include <vector>

#include <boost/uuid/uuid.hpp>

#include <userver/components/loggable_component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/redis/client.hpp>

namespace social_net_service::post
{
    struct FeedPost
    {
        boost::uuids::uuid post_id;
        std::string text;
        boost::uuids::uuid author_user_id;
    };

    class PostFeedCache final : public userver::components::LoggableComponentBase
    {
    public:
        static constexpr std::string_view kName = "post-feed-cache";

        PostFeedCache(const userver::components::ComponentConfig&,
                      const userver::components::ComponentContext&);

        static userver::yaml_config::Schema GetStaticConfigSchema();

        std::vector<FeedPost> GetFeed(const boost::uuids::uuid& user_id, int offset, int limit);
        void OnPostCreated(const boost::uuids::uuid& author_id,
                           const boost::uuids::uuid& post_id,
                           const std::string& text);
        void InvalidateFeedsOfFollowersOf(const boost::uuids::uuid& author_id);
        void InvalidateFeed(const boost::uuids::uuid& user_id);

    private:
        std::deque<FeedPost> LoadFeedFromDb(const boost::uuids::uuid& user_id);
        std::vector<boost::uuids::uuid> GetFollowers(const boost::uuids::uuid& author_id);
        static std::string MakeKey(const boost::uuids::uuid& user_id);
        static std::string SerializePost(const FeedPost& post);
        static FeedPost DeserializePost(const std::string& data);

        userver::storages::postgres::ClusterPtr pg_cluster_;
        std::shared_ptr<userver::storages::redis::Client> redis_client_;
        static constexpr std::size_t kMaxFeedSize = 1000;
    };
} // social_net_service::post
