cmake_minimum_required(VERSION 3.17)

# ==============================================================================
# 專案定義
# ==============================================================================
project(
    Onyx
    VERSION 0.1.0
    DESCRIPTION "Low-latency trading engine for TAIFEX"
    LANGUAGES CXX
)

# ==============================================================================
# CMake 模組路徑
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ==============================================================================
# 專案選項
# ==============================================================================

# --- 建置選項 ---
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" ON)

# --- 編譯器警告選項 ---
option(ENABLE_WARNINGS "Enable compiler warnings" ON)
option(WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

# --- 優化選項 ---
option(ENABLE_LTO "Enable Link Time Optimization (Full LTO)" OFF)

# --- Sanitizer 選項 ---
option(ENABLE_ASAN "Enable AddressSanitizer (memory errors)" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (UB detection)" OFF)
option(
    ENABLE_TSAN
    "Enable ThreadSanitizer (data race detection, mutually exclusive with ASAN)"
    OFF
)

# --- Coverage ---
option(ENABLE_COVERAGE "Enable code coverage flags" OFF)

# --- 開發工具選項 ---
option(ENABLE_CCACHE "Use ccache if available" ON)

# ==============================================================================
# 全局變數
# ==============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 建置類型預設值
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Build type not specified, defaulting to Release")
endif()

# ==============================================================================
# ccache 支援（加速重複編譯）
# ==============================================================================
if(ENABLE_CCACHE)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    else()
        message(STATUS "ccache not found, continuing without it")
    endif()
endif()

# ==============================================================================
# 載入配置模組
# ==============================================================================
include(CompilerOptions) # 編譯器警告、C++ 標準、基本優化
include(CompilerOptimizations) # LTO / PGO 進階優化
include(Sanitizers) # ASAN / UBSAN / TSAN
include(Dependencies) # 第三方依賴

# ==============================================================================
# 子專案
# ==============================================================================
add_subdirectory(common)

# ==============================================================================
# 專案資訊輸出
# ==============================================================================
include(ProjectInfo)
