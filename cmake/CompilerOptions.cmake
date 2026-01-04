# ==============================================================================
# CompilerOptions.cmake
# ==============================================================================
# 功能：設定編譯器警告、C++ 標準、基本優化選項
#
# 建立 INTERFACE library 'project-compiler-options' 來傳播編譯選項至所有 target
# 使用方式：target_link_libraries(your_target PRIVATE project-compiler-options)
# ==============================================================================

message(STATUS "Loading CompilerOptions module")

# ==============================================================================
# 建立 INTERFACE Library
# ==============================================================================
# INTERFACE library 不會產生實際的編譯目標，僅用於傳播編譯選項
add_library(project-compiler-options INTERFACE)

# 別名（方便使用 namespace 風格）
add_library(project::compiler-options ALIAS project-compiler-options)

# ==============================================================================
# C++ 標準要求
# ==============================================================================
# 使用 compile_features 而非 CMAKE_CXX_STANDARD 可確保跨編譯器一致性
target_compile_features(project-compiler-options INTERFACE cxx_std_20)

# ==============================================================================
# 編譯器警告設定
# ==============================================================================
if(ENABLE_WARNINGS)
    message(STATUS "Enabling compiler warnings")

    # ---------------------------------------------------------------------------
    # 通用警告選項（GCC 和 Clang 都支援）
    # ---------------------------------------------------------------------------
    target_compile_options(
        project-compiler-options
        INTERFACE
            -Wall # 基本警告集
            -Wextra # 額外警告
            -Wpedantic # 嚴格 ISO C++ 標準遵循
            -Wconversion # 隱式型別轉換警告（如 int -> char）
            -Wsign-conversion # 有號/無號轉換警告
            -Wdouble-promotion # float 自動提升為 double 警告
            -Wold-style-cast # C-style cast 警告（應使用 static_cast）
            -Wcast-align # 指標轉換導致對齊問題警告
            -Wshadow # 變數遮蔽警告
            -Wnon-virtual-dtor # 有虛函數但無虛解構子警告
            -Woverloaded-virtual # 虛函數遮蔽警告
            -Wnull-dereference # 可能的空指標解參考
            -Wmisleading-indentation # 誤導性縮排（如 if 後無大括號）
            -Wformat=2 # printf/scanf 格式字串檢查（最嚴格等級）
            # 條件式啟用 -Werror（將警告視為錯誤）
            $<$<BOOL:${WARNINGS_AS_ERRORS}>:-Werror>
    )

    # ---------------------------------------------------------------------------
    # Clang 專屬警告
    # ---------------------------------------------------------------------------
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Enabling Clang-specific warnings")

        target_compile_options(
            project-compiler-options
            INTERFACE
                # === 執行緒安全分析 ===
                -Wthread-safety # 基本執行緒安全檢查
                -Wthread-safety-analysis # 完整分析
                -Wthread-safety-precise # 精確模式（減少誤報）
                # === 生命週期與記憶體安全 ===
                -Wdangling # dangling pointer/reference 警告
                -Wdangling-field # dangling field initializer（C++20）
                # === 效能相關警告 ===
                -Wover-aligned # 過度對齊警告（可能浪費記憶體）
                -Wpessimizing-move # 不必要的 std::move（會阻礙 RVO）
                -Wredundant-move # 冗餘的 move（已是 rvalue）
                -Wreturn-std-move # 建議使用 std::move 的情況
                # === 允許必要的 GNU extensions ===
                -Wno-gnu-statement-expression-from-macro-expansion # TRY macro 需要 statement expressions
        )
    endif()

    # ---------------------------------------------------------------------------
    # GCC 專屬警告
    # ---------------------------------------------------------------------------
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(STATUS "Enabling GCC-specific warnings")

        target_compile_options(
            project-compiler-options
            INTERFACE
                -Wuseless-cast # 無意義的 cast（如 static_cast<int>(int_var)）
                -Wlogical-op # 邏輯運算子誤用（如 if (a && a)）
                -Wduplicated-cond # 重複的條件分支
                -Wduplicated-branches # 重複的分支內容
        )
    endif()
endif()

# ==============================================================================
# 通用編譯選項（不受 ENABLE_WARNINGS 影響）
# ==============================================================================
target_compile_options(
    project-compiler-options
    INTERFACE
        -fdiagnostics-color=always # 彩色診斷輸出（提升可讀性）
        -fno-exceptions # 禁止使用異常（低延遲系統常見設定）
        -fno-rtti # 禁止 RTTI（減少二進制大小與虛表開銷）
        # === Debug 配置 ===
        $<$<CONFIG:Debug>:
        -g3 # 最詳細的調試資訊（包含巨集定義）
        -O0 # 優化但保持可調試性（比 -O0 快但不破壞調試）
        -fno-omit-frame-pointer # 保留 frame pointer（profiling 必要）
        >
        # === RelWithDebInfo 配置 ===
        # 適合效能分析（profiling）與除錯的混合模式
        $<$<CONFIG:RelWithDebInfo>:
        -O3 # 最高優化等級
        -march=native # 針對本機 CPU 優化 (other: -march=x86-64-v3)
        -g # 包含調試資訊
        -fno-omit-frame-pointer # 保留 frame pointer（便於 perf 分析）
        >
        # === Release 配置 ===
        $<$<CONFIG:Release>:
        -O3 # 最高優化等級
        -march=native # 針對本機 CPU 優化 (other: -march=x86-64-v3)
        -DNDEBUG # 禁用 assert
        >
)
