# ─── NetBucket tests CMakeLists ───────────────────────────────────────────────

# ─── Helper macro: create a test executable ───────────────────────────────────
macro(nb_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    target_link_libraries(${NAME} PRIVATE
        netbucket_core
        GTest::gtest_main
    )
    gtest_discover_tests(${NAME})
endmacro()

# ─── Phase 0: Scaffold ────────────────────────────────────────────────────────
nb_test(test_scaffold unit/test_scaffold.cpp)

# ─── Phase 1: TokenBucket ────────────────────────────────────────────────────
nb_test(test_token_bucket unit/test_token_bucket.cpp)

# ─── Phase 2: PacketQueue ────────────────────────────────────────────────────
nb_test(test_packet_queue unit/test_packet_queue.cpp)

# ─── Phase 3: TrafficController ──────────────────────────────────────────────
nb_test(test_traffic_controller unit/test_traffic_controller.cpp)

# ─── Phase 4: Config + CLI ───────────────────────────────────────────────────
nb_test(test_config unit/test_config.cpp)

# ─── Phase 10: Classifier ────────────────────────────────────────────────────
nb_test(test_classifier unit/test_classifier.cpp)
