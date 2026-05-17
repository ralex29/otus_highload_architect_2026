cmake_minimum_required(VERSION 3.12...3.31)
project(counter_service CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DownloadUserver)

find_package(
    userver
    COMPONENTS core grpc redis rabbitmq
    QUIET
)
if(NOT userver_FOUND)
    download_userver(TRY_DIR third_party/userver)
endif()

userver_setup_environment()

include_directories(src)

userver_add_grpc_library(
    ${PROJECT_NAME}-proto
    PROTOS counter/v1/counter.proto
    SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../proto
)

add_library(
    ${PROJECT_NAME}_objs OBJECT
    src/counter/counter_service.cpp
    src/counter/counter_service.hpp
    src/counter/counter_consumer.cpp
    src/counter/counter_consumer.hpp
    src/counter/counter_reconciler.cpp
    src/counter/counter_reconciler.hpp
)
target_link_libraries(
    ${PROJECT_NAME}_objs
    PUBLIC
        userver::grpc
        userver::redis
        userver::rabbitmq
        ${PROJECT_NAME}-proto
)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)

include(GNUInstallDirs)
install(
    TARGETS ${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${PROJECT_NAME}
)
install(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/static_config.yaml
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/config_vars.docker.yaml
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}
    COMPONENT ${PROJECT_NAME}
)

userver_testsuite_add_simple()
