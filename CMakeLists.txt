cmake_minimum_required(VERSION 3.17)

# =========================
# 專案定義
# =========================
project(
  TXTradingEngine
  VERSION 0.1.0
  DESCRIPTION "Low-latency trading trading engine for TAIFEX"
  LANGUAGES CXX
)

# =========================
# 全局編譯選項
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

# 啟用編譯器警告
add_compile_options(
  -Wall
  -Wextra
  -Werror
)

# =========================
# 建置類型
# =========================
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-g -O0)        # Debug symbols, 無優化
  add_compile_definitions(DEBUG=1)
else()
  add_compile_options(-O3 -DNDEBUG)  # 最高優化, 關閉 assert
endif()

# =========================
# 第三方依賴
# =========================
include(FetchContent)

# ------------------------
# Google Test
# ------------------------
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()

# ------------------------
# Google Benchmark
# ------------------------
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.3
  )
# 禁用 benchmark 自己程式碼的測試，因為我們用的 compile flags 不一樣
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)


# =========================
# 添加子模塊
# =========================
add_subdirectory(tx-common)

# =========================
# 輸出資訊
# =========================
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "========================================")
