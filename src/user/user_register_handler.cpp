#include "user_register_handler.hpp"

#include "db/data_base.hpp"
#include "password_hasher.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/utils/datetime/date.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace
{
    constexpr std::string_view kFirstNameFieldName = "first_name";
    constexpr std::string_view kSecondNameFieldName = "second_name";
    constexpr std::string_view kBirthdateFieldName = "birthdate";
    constexpr std::string_view kSexFieldName = "sex";
    constexpr std::string_view kBiographyFieldName = "biography";
    constexpr std::string_view kCityFieldName = "city";
    constexpr std::string_view kPasswordFieldName = "password";

    constexpr std::string_view kErrorMembersNotSet = R"(
  {
    "error": "Expected body has `first_name`, `second_name`, `birthdate`, `sex`, `biography`, `city` and `password` fields"
  }
)";

    bool IsCorrectRequest(const userver::formats::json::Value& request_json)
    {
        return     request_json.HasMember(kFirstNameFieldName)
                && request_json.HasMember(kSecondNameFieldName)
                && request_json.HasMember(kBirthdateFieldName)
                && request_json.HasMember(kSexFieldName)
                && request_json.HasMember(kBiographyFieldName)
                && request_json.HasMember(kCityFieldName)
                && request_json.HasMember(kPasswordFieldName);
    }
} // namespace

namespace social_net_service::user
{
    UserRegisterHandler::UserRegisterHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
        : HttpHandlerJsonBase(config, component_context)
          , pg_cluster_(
              component_context.FindComponent<userver::components::Postgres>(DataBase::Name)
                               .GetCluster())
    {
    }

    userver::formats::json::Value UserRegisterHandler::HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_json,
        userver::server::request::RequestContext& context) const
    {
        if (!IsCorrectRequest(request_json))
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return userver::formats::json::FromString(kErrorMembersNotSet);
        }

        const auto [salt, password_hash] = PasswordHasher::HashPassword(request_json[kPasswordFieldName].As<std::string>());
        const auto birthdate = userver::utils::datetime::DateFromRFC3339String(request_json[kBirthdateFieldName].As<std::string>());

        const auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "INSERT INTO social_net_schema.users (first_name, second_name, birthdate, sex, biography, city, password_hash, salt)"
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)"
            "RETURNING user_id",
            request_json[kFirstNameFieldName].As<std::string>(),
            request_json[kSecondNameFieldName].As<std::string>(),
            birthdate,
            request_json[kSexFieldName].As<std::string>(),
            request_json[kBiographyFieldName].As<std::string>(),
            request_json[kCityFieldName].As<std::string>(),
            PasswordHasher::ToHex(password_hash),
            PasswordHasher::ToHex(salt)
            );

        const auto user_id = result.AsSingleRow<boost::uuids::uuid>();
        return userver::formats::json::MakeObject("user_id", boost::uuids::to_string(user_id));

        UASSERT(false);
    }
} // social_net_service::user
