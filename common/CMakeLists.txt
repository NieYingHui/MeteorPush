cmake_minimum_required(VERSION 3.14)

# 公共组件库，封装配置、日志、连接池等通用能力
add_library(MeteorPush_common
    config.cpp
    logging.cpp
    mysql_pool.cpp
    redis_pool.cpp
    kafka_producer.cpp
    kafka_consumer.cpp
    thread_pool.cpp
)

target_include_directories(MeteorPush_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Threads REQUIRED)

target_link_libraries(MeteorPush_common
    PUBLIC
        Threads::Threads
        mysqlclient
        hiredis
        muduo_base
        rdkafka++
        rdkafka
)


