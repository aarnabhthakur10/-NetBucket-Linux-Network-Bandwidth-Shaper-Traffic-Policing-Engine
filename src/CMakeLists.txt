add_library(netbucket_core STATIC
    core/token_bucket.cpp
    core/packet.cpp
    core/packet_queue.cpp
    controller/traffic_controller.cpp
    config/config.cpp
    logger/logger.cpp
    cli/cli.cpp
    networking/tun_device.cpp
    networking/packet_io.cpp
    statistics/statistics.cpp
    classifier/classifier.cpp
)
target_include_directories(netbucket_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(netbucket_core PUBLIC
    nlohmann_json::nlohmann_json
)
find_package(Threads REQUIRED)
target_link_libraries(netbucket_core PUBLIC Threads::Threads)
add_executable(netbucket main.cpp)
target_link_libraries(netbucket PRIVATE
    netbucket_core
)
install(TARGETS netbucket DESTINATION bin)
