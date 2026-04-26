cmake_minimum_required(VERSION 3.12...3.31)
project(social_net_service CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DownloadUserver)

find_package(
    userver
    COMPONENTS core #
               postgresql
               redis
               rabbitmq
               grpc
    QUIET
)
if(NOT userver_FOUND)
    # Tries TRY_DIR first, falls back to downloading userver from GitHub using CPM.
    download_userver(TRY_DIR third_party/userver)
endif()

userver_setup_environment()

# Generate gRPC/protobuf stubs from shared proto contract
userver_add_grpc_library(
    ${PROJECT_NAME}-dialog-proto
    PROTOS chat/v1/dialog.proto
    SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/proto
)

# Common sources
include_directories(src)

add_library(
        ${PROJECT_NAME}_objs OBJECT
        src/greeting.cpp #
        #
        src/auth/login.cpp
        src/auth/auth_bearer.cpp
        src/db/data_base.cpp
        src/user/user_register_handler.cpp
        src/user/password_hasher.cpp
        src/user/user_get_handler.cpp
        src/user/user_search_handler.cpp
        src/user/user_search_handler.hpp
        src/friend/friend_add_handler.cpp
        src/friend/friend_add_handler.hpp
        src/friend/friend_delete_handler.cpp
        src/friend/friend_delete_handler.hpp
        src/post/post_create_handler.cpp
        src/post/post_create_handler.hpp
        src/post/post_update_handler.cpp
        src/post/post_update_handler.hpp
        src/post/post_delete_handler.cpp
        src/post/post_delete_handler.hpp
        src/post/post_get_handler.cpp
        src/post/post_get_handler.hpp
        src/post/post_feed_cache.cpp
        src/post/post_feed_cache.hpp
        src/post/post_feed_handler.cpp
        src/post/post_feed_handler.hpp
        src/post/post_feed_ws_manager.cpp
        src/post/post_feed_ws_manager.hpp
        src/post/post_feed_ws_handler.cpp
        src/post/post_feed_ws_handler.hpp
        src/post/post_feed_publisher.cpp
        src/post/post_feed_publisher.hpp
        src/post/post_feed_consumer.cpp
        src/post/post_feed_consumer.hpp
        src/post/ws_fanout_consumer.cpp
        src/post/ws_fanout_consumer.hpp
        src/dialog/dialog_grpc_client.hpp
        src/dialog/dialog_send_handler.cpp
        src/dialog/dialog_send_handler.hpp
        src/dialog/dialog_list_handler.cpp
        src/dialog/dialog_list_handler.hpp
)
target_link_libraries(
    ${PROJECT_NAME}_objs
    PUBLIC userver::core #
           userver::postgresql
           userver::redis
           userver::rabbitmq
           userver::grpc
           ${PROJECT_NAME}-dialog-proto
)


# The Service
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)

# Unit Tests
add_executable(${PROJECT_NAME}_unittest src/greeting_test.cpp)
target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver::utest)
add_google_tests(${PROJECT_NAME}_unittest)
set_tests_properties(${PROJECT_NAME}_unittest PROPERTIES
    ENVIRONMENT "USERVER_ENABLE_STACK_USAGE_MONITOR=0")

# Benchmarks
add_executable(${PROJECT_NAME}_benchmark src/greeting_benchmark.cpp)
target_link_libraries(${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_NAME}_objs userver::ubench)
add_google_benchmark_tests(${PROJECT_NAME}_benchmark)
set_tests_properties(${PROJECT_NAME}_benchmark PROPERTIES
    ENVIRONMENT "USERVER_ENABLE_STACK_USAGE_MONITOR=0")

# Functional testing
userver_testsuite_add_simple()

# Install
include(GNUInstallDirs)

if(DEFINED ENV{PREFIX})
    message(STATUS "Set install prefix: $ENV{PREFIX}")
    file(TO_CMAKE_PATH "$ENV{PREFIX}" PREFIX_PATH)
    set(CMAKE_INSTALL_PREFIX "${PREFIX_PATH}")
endif()

file(GLOB CONFIGS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.yaml ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json)

install(
    TARGETS ${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${PROJECT_NAME}
)
install(
    FILES ${CONFIGS_FILES}
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}
    COMPONENT ${PROJECT_NAME}
)
