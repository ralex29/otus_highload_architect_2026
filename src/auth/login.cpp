#include "login.hpp"

#include "db/data_base.hpp"
#include "user/password_hasher.hpp"
#include "auth/user_info_cache.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/utils/datetime/date.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>

#include <algorithm>

namespace
{
    constexpr std::string_view kUserIdFieldName = "id";
    constexpr std::string_view kPasswordFieldName = "password";

    constexpr std::string_view kErrorMembersNotSet = R"(
  {
    "error": "Expected body has `id` and `password` fields"
  }
)";

    bool IsCorrectRequest(const userver::formats::json::Value& request_json)
    {
        return     request_json.HasMember(kUserIdFieldName)
                && request_json.HasMember(kPasswordFieldName);
    }

    std::string GenerateBearerToken(size_t length = 32) {
        const std::string charset =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

        std::random_device rd;
        std::mt19937 g(rd());

        std::string token;
        token.reserve(length);

        std::sample(charset.begin(), charset.end(), std::back_inserter(token),
                    length, g);

        return token;
    }
} // namespace

namespace social_net_service::auth
{
    Login::Login(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerJsonBase(config, component_context)
          , pg_cluster_(
              component_context.FindComponent<userver::components::Postgres>(DataBase::Name)
                               .GetCluster())
          , auth_cache_(component_context.FindComponent<AuthCache>())
    {
    }

    std::tuple< std::string, bool> Login::GenerateToken( const boost::uuids::uuid& user_id ) const
    {
        const userver::server::auth::UserAuthInfo::Ticket token { GenerateBearerToken() };
        const std::vector<std::string> scopes = {"read","user"};
        const auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "INSERT INTO auth_schema.tokens (token, user_id, scopes) "
            "VALUES ($1, $2, $3) "
            "RETURNING updated",
            token,
            user_id,
            scopes
        );
        if (result.IsEmpty())
        {
            return {"", false};
        }
        else
        {
            auth_cache_.InvalidateAsync(userver::cache::UpdateType::kIncremental);
            return {token.GetUnderlying(), true};
        }
    }

    userver::formats::json::Value Login::HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_json,
        userver::server::request::RequestContext& context) const
    {
        if (!IsCorrectRequest(request_json))
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return userver::formats::json::FromString(kErrorMembersNotSet);
        }
        boost::uuids::string_generator generator;
        const auto user_id = generator( request_json[kUserIdFieldName].As<std::string>());

        const auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT password_hash, salt "
            "FROM social_net_schema.users "
            "WHERE user_id = $1",
            user_id
            );

        if (result.IsEmpty())
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return userver::formats::json::MakeObject();
        }
        auto const hash = result[0]["password_hash"].As<std::string>();
        auto const salt = result[0]["salt"].As<std::string>();
        if (!PasswordHasher::VerifyPassword(request_json[kPasswordFieldName].As<std::string>(),
            PasswordHasher::FromHex(salt),
            PasswordHasher::FromHex(hash)))
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return userver::formats::json::MakeObject();
        }

        auto const [token, success] = GenerateToken( user_id );

        if (!success)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
            return userver::formats::json::MakeObject();
        }

        return userver::formats::json::MakeObject("token", token);

        UASSERT(false);
    }
} // social_net_service::au
