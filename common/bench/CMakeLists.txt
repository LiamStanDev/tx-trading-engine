# ============================
# 定義 Target
# ============================
add_executable(onyx-common-bench)

# ============================
# Source files
# ============================
target_sources(
    onyx-common-bench
    PRIVATE
        ./core/object_pool_bench.cpp
        ./ipc/shared_memory_bench.cpp
        ./sync/spsc_queue_bench.cpp
        ./io/file_io_bench.cpp
)

# ============================
# Target 屬性
# ============================
target_link_libraries(
    onyx-common-bench
    PRIVATE onyx::common benchmark::benchmark benchmark::benchmark_main
)

# Release 模式優化提示
if(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
    message(
        WARNING
        "Benchmarks should be built in Release mode for accurate results. "
        "Current build type: ${CMAKE_BUILD_TYPE}"
    )
endif()

# ============================
# 輸出資訊
# ============================
message(STATUS "onyx-common-bench: configured")
