# ============================
# 定義 Target
# ============================
add_executable(tx-common-bench)
add_executable(tx-common-sizeof-check)

# ============================
# Source files
# ============================
target_sources(
    tx-common-bench
    PRIVATE
        ./core/result_bench.cpp
        ./protocols/fix/parser_bench.cpp
        ./sync/spsc_queue_bench.cpp
)

target_sources(tx-common-sizeof-check PRIVATE ./core/sizeof_check.cpp)

# ============================
# Target 屬性
# ============================
target_link_libraries(
    tx-common-bench
    PRIVATE tx::common benchmark::benchmark benchmark::benchmark_main
)

target_link_libraries(tx-common-sizeof-check PRIVATE tx::common)

# ============================
# C++23 支援檢測（用於 std::expected）
# ============================
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++23" COMPILER_SUPPORTS_CXX23)

if(COMPILER_SUPPORTS_CXX23)
    target_compile_options(tx-common-bench PRIVATE -std=c++23)
    target_compile_options(tx-common-sizeof-check PRIVATE -std=c++23)
    message(STATUS "C++23 enabled for benchmarks (std::expected available)")
else()
    message(
        STATUS
        "C++23 not available, std::expected benchmarks will be disabled"
    )
endif()

# Release 模式優化提示
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release|RelWithDebInfo")
    message(
        WARNING
        "Benchmarks should be built in Release mode for accurate results. "
        "Current build type: ${CMAKE_BUILD_TYPE}"
    )
endif()

# ============================
# 輸出資訊
# ============================
message(STATUS "tx-common-bench: configured")
