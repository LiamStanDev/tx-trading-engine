# GCC/Clang 雙編譯器遷移指南

## 概述

本專案已完成從單一 Clang 工具鏈遷移到 **GCC/Clang 雙編譯器** 支援的配置。

### 主要變更

- ✅ **預設編譯器**: GCC 12 (RHEL 8 相容)
- ✅ **靜態鏈接**: libstdc++ 和 libgcc (僅 GCC)
- ✅ **CPU 目標**: x86-64-v3 (AVX2, FMA)
- ✅ **LTO 優化**: 統一使用 Full LTO (GCC 和 Clang)
- ✅ **覆蓋率工具**: GCC gcov/lcov
- ✅ **保留 Clang**: 用於開發和進階診斷

---

## 可用的 CMake Presets

### GCC Presets（預設，用於生產部署）

| Preset | 用途 | 特性 |
|--------|------|------|
| `debug` | 開發除錯 | ASAN + UBSAN, 無優化 |
| `relwithdebinfo` | 效能分析 | O3 + LTO + Debug Info |
| `release` | 生產部署 | O3 + LTO, 靜態鏈接 |
| `coverage` | 程式碼覆蓋率 | gcov instrumentation |

### Clang Presets（開發使用）

| Preset | 用途 | 特性 |
|--------|------|------|
| `clang-debug` | Clang 除錯 | ASAN + UBSAN, Thread Safety Analysis |
| `clang-relwithdebinfo` | Clang 效能分析 | O3 + LTO + Debug Info |

---

## 快速開始

### 1. 本地開發（使用 Clang）

```bash
# 配置（Clang Debug）
cmake --preset clang-debug

# 編譯
cmake --build --preset clang-debug

# 測試
ctest --test-dir build/clang-debug --output-on-failure
```

### 2. 生產編譯（使用 GCC + 靜態鏈接）

#### 選項 A：本地編譯（需要 GCC 12）

```bash
# 配置
cmake --preset release

# 編譯
cmake --build --preset release
```

#### 選項 B：RHEL 8 容器編譯（推薦）

```bash
# 使用自動化腳本
./scripts/setup-rhel8-container.sh release

# 或手動使用 Makefile
make release
```

---

## 驗證靜態鏈接

編譯完成後，驗證二進制文件：

```bash
# 檢查動態依賴（應該沒有 libstdc++）
ldd build/release/tx-common/bench/tx-common-bench

# 預期輸出：
#   linux-vdso.so.1
#   libpthread.so.0 => /lib64/libpthread.so.0
#   libm.so.6 => /lib64/libm.so.6
#   libc.so.6 => /lib64/libc.so.6
#   （注意：沒有 libstdc++.so.6）

# 檢查 glibc 版本需求（應 ≤ 2.28）
objdump -T build/release/tx-common/bench/tx-common-bench | grep GLIBC_ | sort -u

# 檢查 AVX2/FMA 指令
objdump -d build/release/tx-common/bench/tx-common-bench | grep -E "vfmadd|vbroadcast" | head -5
```

---

## Makefile 快捷命令

```bash
# 編譯（互動選擇 preset）
make build

# 直接編譯特定 preset
make debug           # GCC Debug
make release         # GCC Release
make clang-debug     # Clang Debug

# 測試
make test

# 基準測試
make bench

# 程式碼覆蓋率（GCC gcov）
make coverage         # 終端報告
make coverage-html    # HTML 報告

# 清理
make clean
```

---

## 編譯器選擇建議

### 使用 GCC 的情況

- ✅ 生產環境部署（需要靜態鏈接）
- ✅ RHEL 8 目標系統
- ✅ 最終發布版本
- ✅ 程式碼覆蓋率分析

### 使用 Clang 的情況

- ✅ 日常開發（更好的錯誤訊息）
- ✅ 需要 Thread Safety Analysis
- ✅ 需要更快的編譯速度
- ✅ 除錯複雜的語義問題

---

## 技術細節

### 靜態鏈接實現

在 `cmake/CompilerOptions.cmake` 中：

```cmake
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(
        project-compiler-options
        INTERFACE
            -static-libstdc++  # 靜態鏈接 C++ 標準庫
            -static-libgcc     # 靜態鏈接 GCC 運行時
    )
endif()
```

### CPU 指令集

- **目標**: x86-64-v3 (AVX2, FMA, BMI1, BMI2)
- **相容性**: 支援 2013+ Intel/AMD CPU
- **配置**: `-march=x86-64-v3` (在 `CompilerOptions.cmake`)

### LTO 配置

- **GCC**: Full LTO with `-flto=auto` (自動平行化)
- **Clang**: Full LTO (移除 ThinLTO 選項)
- **啟用**: `ENABLE_LTO=ON` (release 和 relwithdebinfo preset 預設啟用)

---

## 故障排除

### 問題: GCC Toolset 12 未找到

**解決方案（RHEL 8）**:
```bash
sudo yum install gcc-toolset-12
scl enable gcc-toolset-12 bash
```

### 問題: lcov 未找到

**解決方案**:
```bash
# RHEL/CentOS
sudo yum install lcov

# Debian/Ubuntu
sudo apt install lcov
```

### 問題: 編譯後仍然動態鏈接 libstdc++

**檢查步驟**:
1. 確認使用 GCC preset（不是 Clang）
2. 檢查 `ldd` 輸出
3. 確認 `cmake/CompilerOptions.cmake` 包含靜態鏈接選項

---

## 相關文件

- [RHEL 8 容器編譯腳本](../scripts/setup-rhel8-container.sh)
- [CMake Presets 配置](../CMakePresets.json)
- [編譯器選項配置](../cmake/CompilerOptions.cmake)
- [LTO 優化配置](../cmake/CompilerOptimizations.cmake)

---

## 版本歷史

- **2026-01-05**: 完成 GCC/Clang 雙編譯器遷移
  - 添加 GCC 12 支援
  - 實現靜態鏈接 libstdc++/libgcc
  - 統一 LTO 為 Full LTO
  - 切換覆蓋率工具為 gcov/lcov
  - 更新 CPU 目標為 x86-64-v3
