# ==============================================================================
# Sanitizers.cmake
# ==============================================================================
# 功能：配置 Sanitizers（記憶體與執行緒安全檢測工具）
#
# 支援的 Sanitizers：
#   - AddressSanitizer (ASAN)：記憶體錯誤檢測（buffer overflow, use-after-free）
#   - UndefinedBehaviorSanitizer (UBSAN)：未定義行為檢測（整數溢位、空指標解參考）
#   - ThreadSanitizer (TSAN)：資料競爭 (data race) 檢測
#
# 注意事項：
#   - Sanitizers 會大幅降低效能（2-5x）與增加記憶體使用
#   - 僅應在 Debug/Testing 環境啟用
#   - ASAN 與 TSAN 互斥（底層機制衝突）
# ==============================================================================

message(STATUS "Loading Sanitizers module")

# ==============================================================================
# 互斥檢查：ASAN 與 TSAN 不可同時啟用
# ==============================================================================
if(ENABLE_ASAN AND ENABLE_TSAN)
  message(FATAL_ERROR
    "AddressSanitizer (ASAN) and ThreadSanitizer (TSAN) are mutually exclusive.\n"
    "Both rely on shadow memory techniques that conflict with each other.\n"
    "Please enable only one:\n"
    "  - Use ASAN for memory errors (buffer overflow, use-after-free, leaks)\n"
    "  - Use TSAN for concurrency issues (data races, deadlocks)"
  )
endif()

# ==============================================================================
# AddressSanitizer (ASAN)
# ==============================================================================
if(ENABLE_ASAN)
  message(STATUS "Enabling AddressSanitizer (ASAN)")

  # ---------------------------------------------------------------------------
  # ASAN 編譯與連結選項
  # ---------------------------------------------------------------------------
  # -fsanitize=address：啟用 ASAN
  # -fno-omit-frame-pointer：保留 frame pointer（提升 stack trace 精確度）
  # -g：包含調試資訊（顯示完整原始碼位置）
  target_compile_options(project-compiler-options INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=address>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    $<$<CONFIG:Debug>:-g>
  )

  # 連結選項（必須與編譯選項一致）
  target_link_options(project-compiler-options INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=address>
  )

  # ---------------------------------------------------------------------------
  # ASAN 進階選項（可選）
  # ---------------------------------------------------------------------------
  # 若需啟用 Leak Sanitizer（自動偵測記憶體洩漏）
  # target_compile_options(project-compiler-options INTERFACE
  #   $<$<CONFIG:Debug>:-fsanitize=leak>
  # )

  message(STATUS "ASAN runtime options can be set via ASAN_OPTIONS environment variable")
  message(STATUS "Example: export ASAN_OPTIONS=detect_leaks=1:verbosity=1")
endif()

# ==============================================================================
# UndefinedBehaviorSanitizer (UBSAN)
# ==============================================================================
if(ENABLE_UBSAN)
  message(STATUS "Enabling UndefinedBehaviorSanitizer (UBSAN)")

  # ---------------------------------------------------------------------------
  # UBSAN 編譯與連結選項
  # ---------------------------------------------------------------------------
  # -fsanitize=undefined：檢測所有 UB（整數溢位、除以零、空指標等）
  # -fno-sanitize-recover=undefined：遇到 UB 立即終止（而非繼續執行）
  target_compile_options(project-compiler-options INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=undefined>
    $<$<CONFIG:Debug>:-fno-sanitize-recover=undefined>
  )

  target_link_options(project-compiler-options INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=undefined>
  )

  # ---------------------------------------------------------------------------
  # UBSAN 細粒度控制（可選）
  # ---------------------------------------------------------------------------
  # 若只想檢測特定類型的 UB，可使用：
  # -fsanitize=integer          # 整數溢位
  # -fsanitize=null             # 空指標解參考
  # -fsanitize=bounds           # 陣列越界
  # -fsanitize=alignment        # 記憶體對齊錯誤

  message(STATUS "UBSAN will terminate on first undefined behavior")
endif()

# ==============================================================================
# ThreadSanitizer (TSAN)
# ==============================================================================
if(ENABLE_TSAN)
  message(STATUS "Enabling ThreadSanitizer (TSAN)")

  # ---------------------------------------------------------------------------
  # TSAN 編譯與連結選項
  # ---------------------------------------------------------------------------
  # -fsanitize=thread：啟用 TSAN
  # -g：包含調試資訊（顯示資料競爭的原始碼位置）
  target_compile_options(project-compiler-options INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=thread>
    $<$<CONFIG:Debug>:-g>
  )

  target_link_options(project-compiler-options INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=thread>
  )

  # ---------------------------------------------------------------------------
  # TSAN 注意事項
  # ---------------------------------------------------------------------------
  message(STATUS "TSAN requires all code (including libraries) to be compiled with -fsanitize=thread")
  message(STATUS "TSAN runtime options: export TSAN_OPTIONS=second_deadlock_stack=1")

  # TSAN 已知限制：
  # - 不支援 static 變數的執行緒安全初始化檢測（C++11 magic statics）
  # - 記憶體開銷大（5-10x）
  # - 效能開銷大（2-20x，取決於同步操作頻率）
endif()

# ==============================================================================
# Sanitizer 組合建議
# ==============================================================================
if(ENABLE_ASAN AND ENABLE_UBSAN)
  message(STATUS "ASAN + UBSAN enabled (recommended combination for comprehensive testing)")
endif()

# ==============================================================================
# 教學重點說明
# ==============================================================================
# 1. 為何需要同時設定編譯與連結選項？
#    - 編譯階段：插入 instrumentation code（記憶體存取檢查、同步操作追蹤）
#    - 連結階段：連結 Sanitizer runtime library（libasan.so / libtsan.so）
#    - 兩者缺一不可，否則會出現 undefined reference 錯誤
#
# 2. Sanitizer 效能影響：
#    - ASAN：2-3x 記憶體、1.5-3x 執行時間
#    - UBSAN：1.2-2x 執行時間（依檢測項目多寡）
#    - TSAN：5-10x 記憶體、2-20x 執行時間（同步操作越多越慢）
#
# 3. 為何 ASAN 與 TSAN 互斥？
#    - 兩者都使用 "shadow memory" 技術追蹤記憶體狀態
#    - ASAN：每 8 bytes 記憶體對應 1 byte shadow（1:8 mapping）
#    - TSAN：每 8 bytes 記憶體對應 4 bytes shadow（1:2 mapping）
#    - Shadow memory 布局衝突，無法共存
#
# 4. 最佳實踐：
#    - 日常開發：ASAN + UBSAN（捕捉大部分錯誤）
#    - 多執行緒測試：單獨使用 TSAN
#    - CI 測試：分別執行 ASAN/UBSAN 與 TSAN 測試
#    - 生產環境：不應啟用（效能損失過大）
#
# 5. Sanitizer 輸出範例：
#    ASAN 捕捉 buffer overflow：
#      =================================================================
#      ==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
#      READ of size 4 at 0x... thread T0
#          #0 0x... in main example.cpp:42
#
#    TSAN 捕捉 data race：
#      ==================
#      WARNING: ThreadSanitizer: data race (pid=12345)
#        Write of size 4 at 0x... by thread T1:
#          #0 writer() example.cpp:10
#        Previous read of size 4 at 0x... by main thread:
#          #0 reader() example.cpp:20
# ==============================================================================
