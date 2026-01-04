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
