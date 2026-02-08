#include "user_search_handler.hpp"


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
    UserSearchHandler::UserSearchHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context
    )
        : HttpHandlerBase(config, component_context),
          pg_cluster_(component_context.FindComponent<userver::components::Postgres>(DataBase::Name).GetCluster())
    {
    }

    std::string UserSearchHandler::
    HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&)
    const
    {
        std::string first_name, second_name;
        try
        {
            first_name = request.GetArg("first_name") + "%";
            second_name = request.GetArg("last_name") + "%";
        }
        catch (const std::runtime_error& e)
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return {};
        }
        LOG_DEBUG() << "received first_name: " << first_name << " second_name: " << second_name;
        const auto result = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            "SELECT user_id, first_name, second_name, birthdate, sex, biography, city "
            "FROM social_net_schema.users "
            "WHERE first_name LIKE $1 AND second_name LIKE $2 "
            "ORDER BY user_id",
            first_name,
            second_name
        );

        if (result.IsEmpty())
        {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return {};
        }

        userver::formats::json::ValueBuilder json_array(userver::formats::json::Type::kArray);
        for ( const auto& row: result)
        {
            auto birthdate = row["birthdate"].As<userver::storages::postgres::Date>();
            auto id = row["user_id"].As<boost::uuids::uuid>();
            userver::formats::json::ValueBuilder json_user;
            json_user[ "id" ] = boost::uuids::to_string(id);
            json_user[ "first_name" ] = row["first_name"].As<std::string>();
            json_user[ "second_name" ] = row["second_name"].As<std::string>();
            json_user[ "birthday" ] = userver::utils::datetime::ToString(birthdate);
            json_user[ "sex"] = row["sex"].As<std::string>();
            json_user[ "biography" ] = row["biography"].As<std::string>();
            json_user[ "city" ] = row["city"].As<std::string>();
            json_array.PushBack( json_user.ExtractValue() );
        }
        auto array_str = userver::formats::json::ToString(json_array.ExtractValue());
        LOG_DEBUG() << "result == " << array_str;
        return array_str;
    }

} // social_net_service::user