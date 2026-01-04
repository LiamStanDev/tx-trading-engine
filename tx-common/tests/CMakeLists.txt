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
        ./core/result_test.cpp
        ./ipc/shared_memory_test.cpp
        ./protocols/fix/parser_test.cpp
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
