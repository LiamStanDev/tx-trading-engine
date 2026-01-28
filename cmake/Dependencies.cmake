# ==============================================================================
# Dependencies.cmake
# ==============================================================================
message(STATUS "Loading Dependencies module")

# ==============================================================================
# fmt - 高效能格式化函式庫
# ==============================================================================
find_package(fmt CONFIG REQUIRED)
message(STATUS "Found fmt: ${fmt_VERSION}")

# ==============================================================================
# Google Test - 單元測試框架
# ==============================================================================
if(BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    enable_testing()
    include(GoogleTest)
    message(STATUS "Found GTest: ${GTest_VERSION}")
endif()

# ==============================================================================
# Google Benchmark - 效能測試框架
# ==============================================================================
if(BUILD_BENCHMARKS)
    find_package(benchmark CONFIG REQUIRED)
    message(STATUS "Found benchmark: ${benchmark_VERSION}")
endif()
