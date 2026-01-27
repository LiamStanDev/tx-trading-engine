cmake_minimum_required(VERSION 3.17)

# ==============================================================================
# 專案定義
# ==============================================================================
project(
    TXTradingEngine
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
# CPM.cmake 套件管理器 (Note: 需要設定在 ccache 之後，否則很久)
# ==============================================================================
include(CPM)

# 設定 CPM 快取目錄
if(NOT DEFINED ENV{CPM_SOURCE_CACHE})
    set(ENV{CPM_SOURCE_CACHE} "$ENV{HOME}/.cache/CPM")
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
add_subdirectory(tx-common)

# ==============================================================================
# compile_commands.json 符號連結
# ==============================================================================
# 自動將 compile_commands.json 放到專案根目錄（方便 LSP 找到）
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    set(SOURCE_JSON "${CMAKE_BINARY_DIR}/compile_commands.json")
    set(TARGET_JSON "${CMAKE_SOURCE_DIR}/compile_commands.json")

    # 在 Configure 階段執行
    if(NOT EXISTS "${TARGET_JSON}" OR IS_SYMLINK "${TARGET_JSON}")
        execute_process(
            COMMAND
                ${CMAKE_COMMAND} -E create_symlink "${SOURCE_JSON}"
                "${TARGET_JSON}"
            RESULT_VARIABLE symlink_result
        )

        if(symlink_result EQUAL 0)
            message(STATUS "Created symlink: ${TARGET_JSON} -> ${SOURCE_JSON}")
        else()
            message(
                WARNING
                "Failed to create symlink for compile_commands.json"
            )
        endif()
    endif()
endif()

# ==============================================================================
# 專案資訊輸出
# ==============================================================================
include(ProjectInfo)
