# ========================
# 測試檔
# ========================
set(TEST_SOURCES
  ./core/price_test.cpp
  ./core/result_test.cpp
  ./ipc/shared_memory_test.cpp
)

# ========================
# 測試 Target
# ========================
add_executable(tx-common-tests ${TEST_SOURCES})


# ========================
# 依賴鏈接
# ========================
target_link_libraries(tx-common-tests
  tx-common
  GTest::gtest
  GTest::gtest_main
)

# ========================
# 註冊測試
# ========================
include(GoogleTest)
gtest_discover_tests(tx-common-tests)


# =========================
# 輸出資訊
# =========================
message(STATUS "tx-common-tests: ${TEST_SOURCES}")
