# ==============================================================================
# CompilerOptimizations.cmake
# ==============================================================================
# 功能：配置進階編譯優化（LTO）
#
# LTO (Link Time Optimization)：
#   - 在連結階段進行跨編譯單元優化
#   - GCC 和 Clang 都使用 Full LTO
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
        # -------------------------------------------------------------------------
        # Full LTO 配置（GCC 和 Clang 通用）
        # -------------------------------------------------------------------------
        message(STATUS "Using Full LTO for ${CMAKE_CXX_COMPILER_ID}")

        # 啟用 CMake 內建的 IPO 支援
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)

        # GCC 特定優化：自動平行化 LTO
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            # -flto=auto：自動偵測可用 CPU 核心數進行平行化
            target_compile_options(
                project-compiler-options
                INTERFACE
                    $<$<CONFIG:Release>:-flto=auto>
                    $<$<CONFIG:RelWithDebInfo>:-flto=auto>
            )
            target_link_options(
                project-compiler-options
                INTERFACE
                    $<$<CONFIG:Release>:-flto=auto>
                    $<$<CONFIG:RelWithDebInfo>:-flto=auto>
            )
        endif()

        message(STATUS "LTO/IPO enabled for Release and RelWithDebInfo builds")
    endif()
endif()
