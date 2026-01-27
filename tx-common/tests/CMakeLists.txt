# ============================
# 定義 Target
# ============================
add_executable(tx-common-tests)

# ============================
# Source files
# ============================
target_sources(
    tx-common-tests
    PRIVATE
        ./core/price_test.cpp
        ./ipc/shared_memory_test.cpp
        # ./net/taifex/parser_test.cpp
        ./sync/spsc_queue_test.cpp
        ./io/file_test.cpp
        ./io/buf_reader_test.cpp
        ./sys/cpu_affinity_test.cpp
)

# ============================
# Target 屬性
# ============================
target_link_libraries(
    tx-common-tests
    PRIVATE tx::common GTest::gtest GTest::gtest_main
)

# ============================
# 測試發現與註冊
# ============================
gtest_discover_tests(tx-common-tests)

# ============================
# 輸出資訊
# ============================
message(STATUS "tx-common-tests: configured")
