# ============================
# 定義 Target
# ============================
add_executable(tx-common-bench)

# ============================
# Source files
# ============================
target_sources(tx-common-bench
  PRIVATE
    ./core/result_bench.cpp
)

# ============================
# Target 屬性
# ============================
target_link_libraries(tx-common-bench
  PRIVATE
    tx::common
    benchmark::benchmark
    benchmark::benchmark_main
)

# Release 模式優化提示
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(WARNING
    "Benchmarks should be built in Release mode for accurate results. "
    "Current build type: ${CMAKE_BUILD_TYPE}"
  )
endif()

# ============================
# 輸出資訊
# ============================
message(STATUS "tx-common-bench: configured")
