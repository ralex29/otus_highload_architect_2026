#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <userver/components/loggable_component_base.hpp>
#include <userver/concurrent/mpsc_queue.hpp>
#include <userver/concurrent/variable.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace social_net_service::post {

struct WsSession {
    using Queue = userver::concurrent::MpscQueue<std::string>;
    WsSession() : queue(Queue::Create()), consumer(queue->GetConsumer()) {}
    std::shared_ptr<Queue> queue;
    Queue::Consumer        consumer;
};

class PostFeedWsManager final : public userver::components::LoggableComponentBase {
public:
    static constexpr std::string_view kName = "post-feed-ws-manager";
    PostFeedWsManager(const userver::components::ComponentConfig&,
                      const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    std::shared_ptr<WsSession> Subscribe(const boost::uuids::uuid& user_id);
    void Unsubscribe(const boost::uuids::uuid& user_id,
                     const std::shared_ptr<WsSession>& session);
    void NotifyFollowers(const boost::uuids::uuid& author_id,
                         const boost::uuids::uuid& post_id,
                         const std::string& text);

private:
    std::vector<boost::uuids::uuid> GetFollowers(const boost::uuids::uuid& author_id);

    userver::storages::postgres::ClusterPtr pg_cluster_;

    using SessionList = std::vector<std::weak_ptr<WsSession>>;
    using SessionMap  = std::unordered_map<boost::uuids::uuid, SessionList,
                                           boost::hash<boost::uuids::uuid>>;
    userver::concurrent::Variable<SessionMap> sessions_;
};

} // namespace social_net_service::post
