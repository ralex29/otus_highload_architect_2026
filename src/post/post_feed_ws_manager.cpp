#include "post_feed_ws_manager.hpp"

#include "db/data_base.hpp"

#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace social_net_service::post {

PostFeedWsManager::PostFeedWsManager(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& component_context)
    : LoggableComponentBase(config, component_context)
    , pg_cluster_(
          component_context.FindComponent<userver::components::Postgres>(DataBase::Name)
                           .GetCluster())
{
}

userver::yaml_config::Schema PostFeedWsManager::GetStaticConfigSchema()
{
    return userver::yaml_config::MergeSchemas<LoggableComponentBase>(R"(
type: object
description: WebSocket manager for real-time post feed notifications
additionalProperties: false
properties: {}
)");
}

std::shared_ptr<WsSession> PostFeedWsManager::Subscribe(const boost::uuids::uuid& user_id)
{
    auto session = std::make_shared<WsSession>();
    auto lock = sessions_.Lock();
    (*lock)[user_id].push_back(session);
    return session;
}

void PostFeedWsManager::Unsubscribe(const boost::uuids::uuid& user_id,
                                    const std::shared_ptr<WsSession>& session)
{
    auto lock = sessions_.Lock();
    auto it = lock->find(user_id);
    if (it == lock->end()) return;
    auto& list = it->second;
    list.erase(
        std::remove_if(list.begin(), list.end(),
            [&](const auto& wp) { return wp.expired() || wp.lock() == session; }),
        list.end());
    if (list.empty()) lock->erase(it);
}

void PostFeedWsManager::NotifyFollowers(const boost::uuids::uuid& author_id,
                                         const boost::uuids::uuid& post_id,
                                         const std::string& text)
{
    userver::formats::json::ValueBuilder b;
    b["postId"]         = boost::uuids::to_string(post_id);
    b["postText"]       = text;
    b["author_user_id"] = boost::uuids::to_string(author_id);
    const auto msg = userver::formats::json::ToString(b.ExtractValue());

    auto followers = GetFollowers(author_id);
    auto lock = sessions_.Lock();
    for (const auto& fid : followers) {
        auto it = lock->find(fid);
        if (it == lock->end()) continue;
        for (const auto& wp : it->second) {
            if (auto sp = wp.lock())
                (void)sp->queue->GetMultiProducer().PushNoblock(std::string{msg});
        }
    }
}

std::vector<boost::uuids::uuid> PostFeedWsManager::GetFollowers(
    const boost::uuids::uuid& author_id)
{
    const auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT user_id FROM social_net_schema.friendships WHERE friend_id = $1",
        author_id
    );
    return result.AsContainer<std::vector<boost::uuids::uuid>>();
}

} // namespace social_net_service::post
