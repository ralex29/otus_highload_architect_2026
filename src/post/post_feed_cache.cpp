#include "post_feed_cache.hpp"

#include "db/data_base.hpp"

#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace social_net_service::post
{
    PostFeedCache::PostFeedCache(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : LoggableComponentBase(config, component_context)
        , pg_cluster_(
              component_context.FindComponent<userver::components::Postgres>(DataBase::Name)
                               .GetCluster())
        , redis_client_(
              component_context.FindComponent<userver::components::Redis>("key-value-database")
                               .GetClient("feed-redis"))
    {
    }

    userver::yaml_config::Schema PostFeedCache::GetStaticConfigSchema()
    {
        return userver::yaml_config::MergeSchemas<LoggableComponentBase>(R"(
type: object
description: Redis-backed cache for post feeds
additionalProperties: false
properties: {}
)");
    }

    std::string PostFeedCache::MakeKey(const boost::uuids::uuid& user_id)
    {
        return "feed:" + boost::uuids::to_string(user_id);
    }

    std::string PostFeedCache::SerializePost(const FeedPost& post)
    {
        userver::formats::json::ValueBuilder builder;
        builder["post_id"] = boost::uuids::to_string(post.post_id);
        builder["text"] = post.text;
        builder["author_user_id"] = boost::uuids::to_string(post.author_user_id);
        return userver::formats::json::ToString(builder.ExtractValue());
    }

    FeedPost PostFeedCache::DeserializePost(const std::string& data)
    {
        auto json = userver::formats::json::FromString(data);
        boost::uuids::string_generator gen;
        return FeedPost{
            gen(json["post_id"].As<std::string>()),
            json["text"].As<std::string>(),
            gen(json["author_user_id"].As<std::string>())
        };
    }

    std::vector<FeedPost> PostFeedCache::GetFeed(
        const boost::uuids::uuid& user_id, int offset, int limit)
    {
        const auto key = MakeKey(user_id);

        auto exists = redis_client_->Exists(key, {}).Get();
        if (exists > 0)
        {
            auto raw = redis_client_->Lrange(
                key,
                static_cast<int64_t>(offset),
                static_cast<int64_t>(offset + limit - 1),
                {}
            ).Get();

            std::vector<FeedPost> result;
            result.reserve(raw.size());
            for (const auto& item : raw)
                result.push_back(DeserializePost(item));
            return result;
        }

        // Cache miss — load from DB and populate Redis
        auto db_feed = LoadFeedFromDb(user_id);
        if (!db_feed.empty())
        {
            std::vector<std::string> serialized;
            serialized.reserve(db_feed.size());
            for (const auto& post : db_feed)
                serialized.push_back(SerializePost(post));

            redis_client_->Rpush(key, std::move(serialized), {}).Get();
            redis_client_->Ltrim(key, 0, static_cast<int64_t>(kMaxFeedSize - 1), {}).Get();
        }

        // Return the requested slice
        std::vector<FeedPost> result;
        auto begin = static_cast<std::size_t>(offset);
        if (begin >= db_feed.size())
            return result;
        auto end = std::min(begin + static_cast<std::size_t>(limit), db_feed.size());
        result.assign(db_feed.begin() + begin, db_feed.begin() + end);
        return result;
    }

    void PostFeedCache::OnPostCreated(
        const boost::uuids::uuid& author_id,
        const boost::uuids::uuid& post_id,
        const std::string& text)
    {
        auto followers = GetFollowers(author_id);

        FeedPost new_post{post_id, text, author_id};
        auto serialized = SerializePost(new_post);

        for (const auto& follower_id : followers)
        {
            const auto key = MakeKey(follower_id);
            auto exists = redis_client_->Exists(key, {}).Get();
            if (exists > 0)
            {
                redis_client_->Lpush(key, serialized, {}).Get();
                redis_client_->Ltrim(key, 0, static_cast<int64_t>(kMaxFeedSize - 1), {}).Get();
            }
        }
    }

    void PostFeedCache::InvalidateFeedsOfFollowersOf(const boost::uuids::uuid& author_id)
    {
        auto followers = GetFollowers(author_id);
        if (followers.empty())
            return;

        std::vector<std::string> keys;
        keys.reserve(followers.size());
        for (const auto& follower_id : followers)
            keys.push_back(MakeKey(follower_id));

        redis_client_->Del(std::move(keys), {}).Get();
    }

    void PostFeedCache::InvalidateFeed(const boost::uuids::uuid& user_id)
    {
        redis_client_->Del(MakeKey(user_id), {}).Get();
    }

    std::deque<FeedPost> PostFeedCache::LoadFeedFromDb(const boost::uuids::uuid& user_id)
    {
        const auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT p.post_id, p.text, p.author_user_id "
            "FROM social_net_schema.posts p "
            "INNER JOIN social_net_schema.friendships f ON f.friend_id = p.author_user_id "
            "WHERE f.user_id = $1 "
            "ORDER BY p.created_at DESC "
            "LIMIT 1000",
            user_id
        );

        std::deque<FeedPost> feed;
        for (const auto& row : result)
        {
            feed.push_back(FeedPost{
                row["post_id"].As<boost::uuids::uuid>(),
                row["text"].As<std::string>(),
                row["author_user_id"].As<boost::uuids::uuid>()
            });
        }
        return feed;
    }

    std::vector<boost::uuids::uuid> PostFeedCache::GetFollowers(const boost::uuids::uuid& author_id)
    {
        const auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT user_id FROM social_net_schema.friendships "
            "WHERE friend_id = $1",
            author_id
        );

        return result.AsContainer<std::vector<boost::uuids::uuid>>();
    }
} // social_net_service::post
