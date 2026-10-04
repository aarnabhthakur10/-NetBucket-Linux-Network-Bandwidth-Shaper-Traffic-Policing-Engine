#  NetBucket source CMakeLists 

#  Core library 
add_library(netbucket_core STATIC
    # Phase 1: TokenBucket
    core/token_bucket.cpp

    # Phase 2: Packet + Queue
    core/packet.cpp
    core/packet_queue.cpp

    # Phase 3: Traffic Controller
    controller/traffic_controller.cpp

    # Phase 4: Config, Logger, CLI
    config/config.cpp
    logger/logger.cpp
    cli/cli.cpp

    # Phase 5: Linux Networking
    networking/tun_device.cpp
    networking/packet_io.cpp

    # Phase 9: Statistics
    statistics/statistics.cpp

    # Phase 10: Classifier
    classifier/classifier.cpp
)

target_include_directories(netbucket_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(netbucket_core PUBLIC
    nlohmann_json::nlohmann_json
)

# Threads (for std::thread, std::mutex, std::condition_variable)
find_package(Threads REQUIRED)
target_link_libraries(netbucket_core PUBLIC Threads::Threads)

# Main executable 
add_executable(netbucket main.cpp)

target_link_libraries(netbucket PRIVATE
    netbucket_core
)

# Install 
install(TARGETS netbucket DESTINATION bin)
