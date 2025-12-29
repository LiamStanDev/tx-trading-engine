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
# CMake 模組路徑
# =========================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# =========================
# CPM.cmake 套件管理器
# =========================
include(CPM)
# 設定 CPM 快取目錄（跨專案共享依賴）
if(NOT DEFINED ENV{CPM_SOURCE_CACHE})
  set(ENV{CPM_SOURCE_CACHE} "$ENV{HOME}/.cache/CPM")
endif()

# =========================
# 專案選項
# =========================
option(TX_BUILD_TESTS "Build tests" ON)
option(TX_BUILD_BENCHMARKS "Build benchmarks" ON)
option(TX_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(TX_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

# =========================
# 全局設定
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 建置類型預設值
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# =========================
# 編譯器選項 Interface Library
# =========================
add_library(tx-compiler-options INTERFACE)
add_library(tx::compiler-options ALIAS tx-compiler-options)
# C++ 標準要求
target_compile_features(tx-compiler-options INTERFACE cxx_std_20)
# 編譯器警告
if(TX_ENABLE_WARNINGS)
  target_compile_options(tx-compiler-options INTERFACE
    -Wall
    -Wextra
    $<$<BOOL:${TX_WARNINGS_AS_ERRORS}>:-Werror>
  )
endif()

# 建置類型特定選項
target_compile_options(tx-compiler-options INTERFACE
  $<$<CONFIG:Debug>:-g -O0>
  $<$<CONFIG:Release>:-O3 -march=native -flto -DNDEBUG>
  $<$<CONFIG:RelWithDebInfo>:-g -O2 -DNDEBUG>
  $<$<CONFIG:MinSizeRel>:-Os -DNDEBUG>
)

# Debug 模式定義 (可以使用 #ifdef DEBUG 宏)
target_compile_definitions(tx-compiler-options INTERFACE
  $<$<CONFIG:Debug>:DEBUG=1>
)

# =========================
# 第三方依賴
# =========================
# fmt - 格式化庫
CPMAddPackage(
  NAME fmt
  GIT_TAG 12.1.0
  GITHUB_REPOSITORY fmtlib/fmt
  OPTIONS
    "FMT_INSTALL ON"
)

# Google Test
if(TX_BUILD_TESTS)
  CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.14.0
    OPTIONS
      "INSTALL_GTEST OFF"
      "gtest_force_shared_crt ON"
  )
  enable_testing()
  include(GoogleTest)
endif()

# Google Benchmark
if(TX_BUILD_BENCHMARKS)
  CPMAddPackage(
    NAME benchmark
    GITHUB_REPOSITORY google/benchmark
    GIT_TAG v1.8.3
    OPTIONS
      "BENCHMARK_ENABLE_TESTING OFF"
      "BENCHMARK_ENABLE_GTEST_TESTS OFF"
      "BENCHMARK_ENABLE_INSTALL OFF"
  )
endif()

# =========================
# 子專案
# =========================
add_subdirectory(tx-common)


# =========================
# 專案資訊輸出
# =========================
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "----------------------------------------")
message(STATUS "Options:")
message(STATUS "  Build Tests: ${TX_BUILD_TESTS}")
message(STATUS "  Build Benchmarks: ${TX_BUILD_BENCHMARKS}")
message(STATUS "  Enable Warnings: ${TX_ENABLE_WARNINGS}")
message(STATUS "  Warnings as Errors: ${TX_WARNINGS_AS_ERRORS}")
message(STATUS "----------------------------------------")
message(STATUS "CPM Cache: $ENV{CPM_SOURCE_CACHE}")
message(STATUS "========================================")
