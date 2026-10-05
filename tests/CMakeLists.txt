macro(nb_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    target_link_libraries(${NAME} PRIVATE
        netbucket_core
        GTest::gtest_main
    )
    gtest_discover_tests(${NAME})
endmacro()
nb_test(test_scaffold unit/test_scaffold.cpp)
nb_test(test_token_bucket unit/test_token_bucket.cpp)
nb_test(test_packet_queue unit/test_packet_queue.cpp)
nb_test(test_traffic_controller unit/test_traffic_controller.cpp)
nb_test(test_config unit/test_config.cpp)
nb_test(test_classifier unit/test_classifier.cpp)
