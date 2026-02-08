#include "user_get_handler.hpp"


#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

#include <db/data_base.hpp>
#include <userver/utils/datetime/date.hpp>
#include <userver/storages/postgres/component.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>
#include <userver/storages/postgres/io/date.hpp>

namespace
{
    constexpr std::string_view kIdFieldName = "id";
    constexpr std::string_view kFirstNameFieldName = "first_name";
    constexpr std::string_view kSecondNameFieldName = "second_name";
    constexpr std::string_view kBirthdateFieldName = "birthdate";
    constexpr std::string_view kSexFieldName = "sex";
    constexpr std::string_view kBiographyFieldName = "biography";
    constexpr std::string_view kCityFieldName = "city";

    std::string TimePointToString(std::chrono::system_clock::time_point tp, const std::string& format)
    {
        std::time_t tt = std::chrono::system_clock::to_time_t(tp);
        // Use std::gmtime for UTC or std::localtime for local time zone
        std::tm tm = *std::gmtime(&tt);
        std::stringstream ss;
        ss << std::put_time(&tm, format.c_str());
        return ss.str();
    }
}


namespace social_net_service::user
{
    UserGetHandler::UserGetHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context
    )
        : HttpHandlerBase(config, component_context),
          pg_cluster_(component_context.FindComponent<userver::components::Postgres>(DataBase::Name).GetCluster())
    {
    }

    std::string UserGetHandler::
    HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&)
    const
    {
        boost::uuids::string_generator generator;
        boost::uuids::uuid user_id;
        try
        {
            user_id = generator(request.GetPathArg(0));
        }
        catch (const std::runtime_error& e)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return {};
        }
        LOG_DEBUG() << "received id " << user_id;
        const auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT user_id, first_name, second_name, birthdate, sex, biography, city "
            "FROM social_net_schema.users "
            "WHERE user_id = $1",
            user_id
        );

        if (result.IsEmpty())
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return {};
        }
        auto birthdate = result[0]["birthdate"].As<userver::storages::postgres::Date>();
        LOG_DEBUG() << "birthdate " << birthdate;
        auto json_result = userver::formats::json::MakeObject(
            kIdFieldName, boost::uuids::to_string(user_id),
            kFirstNameFieldName, result[0]["first_name"].As<std::string>(),
            kSecondNameFieldName, result[0]["second_name"].As<std::string>(),
            kBirthdateFieldName, userver::utils::datetime::ToString(birthdate),
            kSexFieldName, result[0]["sex"].As<std::string>(),
            kBiographyFieldName, result[0]["biography"].As<std::string>(),
            kCityFieldName, result[0]["city"].As<std::string>()
        );
        LOG_DEBUG() << "result == " << json_result;
        return userver::formats::json::ToString(json_result);
    }
} // social_net_service::user
