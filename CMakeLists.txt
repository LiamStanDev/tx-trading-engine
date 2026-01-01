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
option(TX_ENABLE_LTO "Enable Link Time Optimization" OFF)
option(TX_ENABLE_CCACHE "Use ccache if available" ON)

# Sanitizers
option(TX_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(TX_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(TX_ENABLE_TSAN "Enable ThreadSanitizer (mutually exclusive with ASAN)" OFF)

# Sanitizers 互斥檢查
if(TX_ENABLE_ASAN AND TX_ENABLE_TSAN)
  message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer are mutually exclusive. Please enable only one.")
endif()

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
# ccache 支援
# =========================
if(TX_ENABLE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
  else()
    message(STATUS "ccache not found")
  endif()
endif()

# =========================
# 編譯器選項 Interface Library
# =========================
add_library(tx-compiler-options INTERFACE)
add_library(tx::compiler-options ALIAS tx-compiler-options)

# C++ 標準要求
target_compile_features(tx-compiler-options INTERFACE cxx_std_20)

if(TX_ENABLE_WARNINGS)
  # 1. 通用選項 (GCC 和 Clang 都支援)
  target_compile_options(tx-compiler-options INTERFACE
    -Wall -Wextra
    -Wconversion
    -Wsign-conversion
    -Wdouble-promotion
    -Wold-style-cast
    -Wcast-align
    -Wshadow
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wnull-dereference
    -Wmisleading-indentation
    -Wformat=2
    $<$<BOOL:${TX_WARNINGS_AS_ERRORS}>:-Werror>
  )

  # 2. GCC 專屬
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(tx-compiler-options INTERFACE
      -Wuseless-cast
      -Wlogical-op
      -Wduplicated-cond
      -Wduplicated-branches
    )
  endif()

  # 3. Clang 專屬
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(tx-compiler-options INTERFACE
      -Wthread-safety  # 靜態檢查鎖的正確性
    )
  endif()
endif()

# 編譯選項
target_compile_options(tx-compiler-options INTERFACE
  # Color
  -fdiagnostics-color=always # 彩色診斷輸出
  # Debug + Sanitizers
  $<$<CONFIG:Debug>: -g3 -Og -fno-omit-frame-pointer
    $<$<BOOL:${TX_ENABLE_ASAN}>:-fsanitize=address -fno-omit-frame-pointer>
    $<$<BOOL:${TX_ENABLE_UBSAN}>:-fsanitize=undefined>
    $<$<BOOL:${TX_ENABLE_TSAN}>:-fsanitize=thread>
  >
  # Release + 優化
  # -march=x86-64-v2  # 支持 SSE4.2, POPCNT (2008+)
  # -march=x86-64-v3  # 支持 AVX2, BMI2 (2013+)
  # -march=x86-64-v4  # 支持 AVX512 (2017+)
  $<$<CONFIG:Release>: -O2 -march=native -DNDEBUG>
)
# 鏈接選項（Sanitizers 和 LTO）
target_link_options(tx-compiler-options INTERFACE
  $<$<CONFIG:Debug>:
    $<$<BOOL:${TX_ENABLE_ASAN}>:-fsanitize=address>
    $<$<BOOL:${TX_ENABLE_UBSAN}>:-fsanitize=undefined>
    $<$<BOOL:${TX_ENABLE_TSAN}>:-fsanitize=thread>
  >
)

# =========================
# Link Time Optimization (LTO)
# =========================
if(TX_ENABLE_LTO)
  # 檢查 LTO 支持
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
  if(ipo_supported)
    # 設定全局預設值（所有 target 都會繼承）
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    message(STATUS "LTO/IPO enabled for Release builds (global)")
  else()
    message(WARNING "LTO not supported: ${ipo_error}")
    set(TX_ENABLE_LTO OFF CACHE BOOL "LTO not supported" FORCE)
  endif()
endif()

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
# compile_commands.json 軟連結
# =========================
# 自動將 compile_commands.json 放到項目根目錄
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  # 取得原始路徑與目標路徑
  set(SOURCE_JSON "${CMAKE_BINARY_DIR}/compile_commands.json")
  set(TARGET_JSON "${CMAKE_SOURCE_DIR}/compile_commands.json")

  # 在 Configure 階段直接執行，而不是等待編譯
  if(NOT EXISTS "${TARGET_JSON}" OR IS_SYMLINK "${TARGET_JSON}")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E create_symlink "${SOURCE_JSON}" "${TARGET_JSON}"
      RESULT_VARIABLE symlink_res
    )
  endif()
endif()


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
message(STATUS "  Enable LTO: ${TX_ENABLE_LTO}")
message(STATUS "  Enable ccache: ${TX_ENABLE_CCACHE}")
message(STATUS "  Enable AddressSanitizer: ${TX_ENABLE_ASAN}")
message(STATUS "  Enable UndefinedBehaviorSanitizer: ${TX_ENABLE_UBSAN}")
message(STATUS "  Enable ThreadSanitizer: ${TX_ENABLE_TSAN}")
message(STATUS "----------------------------------------")
message(STATUS "CPM Cache: $ENV{CPM_SOURCE_CACHE}")
message(STATUS "========================================")
