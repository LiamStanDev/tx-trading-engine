# ============================
# 定義 Target
# ============================
add_executable(onyx-common-tests)

# ============================
# Source files
# ============================
target_sources(
    onyx-common-tests
    PRIVATE
        ./error_test.cpp
        ./core/price_test.cpp
        ./core/object_pool_test.cpp
        ./core/order_book_test.cpp
        ./ipc/shared_memory_test.cpp
        ./sync/spsc_queue_test.cpp
        ./io/file_test.cpp
        ./io/socket_test.cpp
        ./db/pg/connection_test.cpp
        ./db/pg/connection_pool_test.cpp
        ./sys/cpu_affinity_test.cpp
        ./net/taifex/codec_test.cpp
        ./net/taifex/parser_i010_test.cpp
        ./net/taifex/parser_i024_test.cpp
        ./net/taifex/parser_i081_test.cpp
        ./net/taifex/parser_i083_test.cpp
)

# ============================
# Target 屬性
# ============================
target_link_libraries(
    onyx-common-tests
    PRIVATE onyx::common GTest::gtest GTest::gtest_main
)

# ============================
# 測試發現與註冊
# ============================
gtest_discover_tests(onyx-common-tests)

# ============================
# 輸出資訊
# ============================
message(STATUS "onyx-common-tests: configured")
