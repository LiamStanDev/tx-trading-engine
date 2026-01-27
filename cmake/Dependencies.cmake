# ==============================================================================
# Dependencies.cmake
# ==============================================================================
# 功能：管理第三方依賴（使用 CPM.cmake）
#
# CPM (CMake Package Manager)：
#   - 自動下載與編譯第三方庫
#   - 支援 GitHub、GitLab、本地路徑
#   - 跨專案快取（透過 CPM_SOURCE_CACHE 環境變數）
# ==============================================================================

message(STATUS "Loading Dependencies module")

# ==============================================================================
# fmt
# ==============================================================================
message(STATUS "Fetching dependency: fmt")

cpmaddpackage(
  NAME fmt
  GIT_TAG 12.1.0
  GITHUB_REPOSITORY fmtlib/fmt
  OPTIONS
    "FMT_INSTALL OFF"           # 允許安裝 fmt（供其他專案使用）
    "FMT_DOC OFF"              # 不生成文件
    "FMT_TEST OFF"             # 不編譯 fmt 的測試
    "BUILD_SHARED_LIBS OFF"    # 強制靜態庫
)

# ==============================================================================
# Google Test - 單元測試框架
# ==============================================================================
if(BUILD_TESTS)
    message(STATUS "Fetching dependency: googletest (for unit testing)")

    cpmaddpackage(
      NAME googletest
      GITHUB_REPOSITORY google/googletest
      GIT_TAG v1.14.0
      OPTIONS
        "INSTALL_GTEST OFF"            # 不安裝 gtest（避免污染系統）
        "gtest_disable_pthreads OFF"   # 啟用 pthread 支援（Linux/macOS）
        "BUILD_SHARED_LIBS OFF"        # 強制靜態庫
    )

    # 啟用 CTest 測試框架
    enable_testing()

    # 引入 GoogleTest CMake 模組（提供 gtest_discover_tests 函數）
    include(GoogleTest)

    message(
        STATUS
        "Google Test enabled - use gtest_discover_tests() to register tests"
    )
else()
    message(STATUS "Skipping Google Test (BUILD_TESTS=OFF)")
endif()

# ==============================================================================
# Google Benchmark - 效能測試框架
# ==============================================================================
if(BUILD_BENCHMARKS)
    message(STATUS "Fetching dependency: benchmark (for performance testing)")

    cpmaddpackage(
      NAME benchmark
      GITHUB_REPOSITORY google/benchmark
      GIT_TAG v1.8.3
      OPTIONS
        "BENCHMARK_ENABLE_TESTING OFF"       # 不執行 benchmark 自己的測試
        "BENCHMARK_ENABLE_INSTALL OFF"       # 不安裝 benchmark
        "BENCHMARK_DOWNLOAD_DEPENDENCIES OFF" # 自動下載依賴（如 gtest）
        "BUILD_SHARED_LIBS OFF"              # 強制靜態庫
    )

    message(
        STATUS
        "Google Benchmark enabled - use BENCHMARK() macro to define benchmarks"
    )
else()
    message(STATUS "Skipping Google Benchmark (BUILD_BENCHMARKS=OFF)")
endif()
