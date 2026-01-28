# ==============================================================================
# ProjectInfo.cmake
# ==============================================================================
# 功能：輸出專案配置資訊（Build Type、編譯器、啟用的選項）
#
# 在 CMake configure 階段顯示關鍵資訊，幫助開發者確認建置配置
# ==============================================================================

message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(
    STATUS
    "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)
message(STATUS "Compiler Path: ${CMAKE_CXX_COMPILER}")
message(STATUS "----------------------------------------")

# ==============================================================================
# 編譯選項
# ==============================================================================
message(STATUS "Build Options:")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "  Build Benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "  Enable Warnings: ${ENABLE_WARNINGS}")
message(STATUS "  Warnings as Errors: ${WARNINGS_AS_ERRORS}")

# ==============================================================================
# 優化選項
# ==============================================================================
message(STATUS "----------------------------------------")
message(STATUS "Optimization Options:")
message(STATUS "  Enable LTO: ${ENABLE_LTO}")

if(ENABLE_LTO AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "    LTO Mode: ${LTO_MODE}")
endif()

# ==============================================================================
# Sanitizer 選項
# ==============================================================================
message(STATUS "----------------------------------------")
message(STATUS "Sanitizer Options:")
message(STATUS "  AddressSanitizer (ASAN): ${ENABLE_ASAN}")
message(STATUS "  UndefinedBehaviorSanitizer (UBSAN): ${ENABLE_UBSAN}")
message(STATUS "  ThreadSanitizer (TSAN): ${ENABLE_TSAN}")

# ==============================================================================
# 工具與快取
# ==============================================================================
message(STATUS "----------------------------------------")
message(STATUS "Build Tools:")
message(STATUS "  Enable ccache: ${ENABLE_CCACHE}")

if(CCACHE_PROGRAM)
    message(STATUS "    ccache path: ${CCACHE_PROGRAM}")
endif()

# ==============================================================================
# 建置目錄資訊
# ==============================================================================
message(STATUS "----------------------------------------")
message(STATUS "Directories:")
message(STATUS "  Source: ${CMAKE_SOURCE_DIR}")
message(STATUS "  Binary: ${CMAKE_BINARY_DIR}")

# ==============================================================================
# 重要提示
# ==============================================================================
if(ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN)
    message(STATUS "----------------------------------------")
    message(STATUS "IMPORTANT: Sanitizers enabled")
    message(STATUS "  Performance will be significantly reduced (2-10x slower)")
    message(STATUS "  Only use in Debug/Testing builds, not in Production")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT ENABLE_LTO)
    message(STATUS "----------------------------------------")
    message(STATUS "HINT: Consider enabling LTO for Release builds")
    message(STATUS "  LTO: -DENABLE_LTO=ON (5-15% performance improvement)")
endif()

message(STATUS "========================================")
