# ============================
# 基準測試可執行文件
# ============================
add_executable(result_bench
    ./core/result_bench.cpp
)

target_link_libraries(result_bench
    PRIVATE
    tx-common
    benchmark::benchmark
    benchmark::benchmark_main
)

# ============================
# 輸出資訊
# ============================
message(STATUS "Benchmarks configured for tx-common")
