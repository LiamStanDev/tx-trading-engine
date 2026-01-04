# ==============================================================================
# CompilerOptimizations.cmake
# ==============================================================================
# 功能：配置進階編譯優化（LTO）
#
# LTO (Link Time Optimization)：
#   - 在連結階段進行跨編譯單元優化
#   - Clang 支援 ThinLTO（平行化，快速）與 Full LTO（優化最佳，較慢）
#   - GCC 僅支援 Full LTO
# ==============================================================================

message(STATUS "Loading CompilerOptimizations module")

# ==============================================================================
# 互斥檢查：PGO_GENERATE 與 PGO_USE 不應同時啟用
# ==============================================================================
if(ENABLE_PGO_GENERATE AND ENABLE_PGO_USE)
    message(
        FATAL_ERROR
        "ENABLE_PGO_GENERATE and ENABLE_PGO_USE are mutually exclusive.\n"
        "PGO workflow:\n"
        "  1. Build with -DENABLE_PGO_GENERATE=ON\n"
        "  2. Run your application with representative workload\n"
        "  3. Rebuild with -DENABLE_PGO_GENERATE=OFF -DENABLE_PGO_USE=ON"
    )
endif()

# ==============================================================================
# 互斥檢查：PGO 與 Sanitizers 不應同時啟用
# ==============================================================================
# 原因：Sanitizer 的 instrumentation 會扭曲 profile 數據
if(
    (ENABLE_PGO_GENERATE OR ENABLE_PGO_USE)
    AND (ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN)
)
    message(
        FATAL_ERROR
        "PGO and Sanitizers are mutually exclusive.\n"
        "Sanitizer instrumentation will distort profile data.\n"
        "Please disable Sanitizers when using PGO."
    )
endif()

# ==============================================================================
# LTO (Link Time Optimization) 配置
# ==============================================================================
if(ENABLE_LTO)
    message(STATUS "Configuring Link Time Optimization (LTO)")

    # 檢查編譯器是否支援 LTO/IPO
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

    if(NOT ipo_supported)
        message(WARNING "LTO not supported by compiler: ${ipo_error}")
        set(ENABLE_LTO OFF CACHE BOOL "LTO not supported" FORCE)
    else()
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # -------------------------------------------------------------------------
            # Clang: 支援 ThinLTO 與 Full LTO
            # -------------------------------------------------------------------------
            if(LTO_MODE STREQUAL "THIN")
                message(
                    STATUS
                    "Using Clang ThinLTO (faster compilation, good optimization)"
                )

                # ThinLTO 需要手動設定編譯與連結選項
                target_compile_options(
                    project-compiler-options
                    INTERFACE $<$<CONFIG:Release>:-flto=thin>
                )
                target_link_options(
                    project-compiler-options
                    INTERFACE $<$<CONFIG:Release>:-flto=thin>
                )
            elseif(LTO_MODE STREQUAL "FULL")
                message(
                    STATUS
                    "Using Clang Full LTO (slower compilation, best optimization)"
                )

                # Full LTO 使用 CMake 內建的 IPO 支援
                set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE
                    TRUE
                    PARENT_SCOPE
                )
                set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO
                    TRUE
                    PARENT_SCOPE
                )
            else()
                message(
                    FATAL_ERROR
                    "Invalid LTO_MODE: ${LTO_MODE}. Must be 'THIN' or 'FULL'"
                )
            endif()
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            # -------------------------------------------------------------------------
            # GCC: 僅支援 Full LTO
            # -------------------------------------------------------------------------
            message(STATUS "Using GCC Full LTO")

            # GCC LTO 使用 CMake 內建的 IPO 支援
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE PARENT_SCOPE)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO
                TRUE
                PARENT_SCOPE
            )

            # GCC LTO 額外選項（可選）
            # -flto=auto：自動偵測可用 CPU 核心數進行平行化
            target_compile_options(
                project-compiler-options
                INTERFACE $<$<CONFIG:Release>:-flto=auto>
            )
            target_link_options(
                project-compiler-options
                INTERFACE $<$<CONFIG:Release>:-flto=auto>
            )
        endif()

        message(STATUS "LTO/IPO enabled for Release builds")
    endif()
endif()
