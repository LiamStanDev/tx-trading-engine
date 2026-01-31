# HFT Benchmark Report - 終端版

## 快速開始

```bash
make bench-report
```

## 報表內容

### 6 大分析維度

| 檔案                      | 工具               | 內容                                   |
|---------------------------|--------------------|----------------------------------------|
| `1_benchmark.txt`         | Google Benchmark   | Mean/Median/StdDev/P99 延遲分佈        |
| `2_perf_stat.txt`         | `perf stat -d -d -d` | IPC, Cache Miss, Branch Miss, TLB Miss |
| `3_perf_hotspots.txt`     | `perf record/report` | Top 20 熱點函數 CPU 佔用比例           |
| `4_cachegrind.txt`        | `valgrind cachegrind` | L1/L2/LL Cache Miss 詳細分析           |
| `5_flamegraph.svg`        | FlameGraph         | 視覺化 CPU 時間分佈                    |
| `6_hft_scorecard.txt`     | 自動萃取           | **關鍵指標摘要（終端直接顯示）**       |

## HFT 優化目標

| 指標                  | 目標值      |
|-----------------------|-------------|
| **IPC**               | > 2.0       |
| **L1 Cache Miss**     | < 3%        |
| **Branch Misprediction** | < 1%     |
| **TLB Miss**          | < 0.01%     |
| **P99 Jitter**        | < 2x Mean   |

## 延遲殺手檢查清單

### 1️⃣ IPC < 1.5
```bash
grep "insn per cycle" reports/bench_xxx/2_perf_stat.txt
```
**可能原因**: 虛擬函數、Cache Miss、Pipeline Stall

**優化手段**:
- 消除虛擬函數 → CRTP 靜態多型
- 減少指令相依性 → Loop Unrolling

### 2️⃣ L1 Cache Miss > 5%
```bash
grep "L1-dcache" reports/bench_xxx/2_perf_stat.txt
```
**可能原因**: 資料佈局問題、False Sharing

**優化手段**:
- AoS → SoA (Structure of Arrays)
- `alignas(64)` 消除 False Sharing
- 分離冷熱資料

### 3️⃣ Branch Misprediction > 2%
```bash
grep "branch-misses" reports/bench_xxx/2_perf_stat.txt
```
**可能原因**: 隨機分支、深層 if-else

**優化手段**:
- 使用 `[[likely]]` / `[[unlikely]]`
- Branchless Programming
- Profile-Guided Optimization (PGO)

## 優化工作流程

```bash
# 1. Baseline
make bench-report
# 報表: reports/bench_all_20260111_100000/

# 2. 實施優化
# - 修改程式碼
# - 調整資料結構佈局

# 3. Verify
make bench-report
# 報表: reports/bench_all_20260111_110000/

# 4. 比對
diff reports/bench_all_20260111_100000/6_hft_scorecard.txt \
     reports/bench_all_20260111_110000/6_hft_scorecard.txt

# 5. Commit
git add .
git commit -m "perf: optimize OrderBook (IPC 1.1→2.1)"
```

## 常用命令

```bash
# 查看完整報表摘要（執行完畢自動顯示）
cat reports/bench_xxx/6_hft_scorecard.txt

# 查看詳細硬體計數器
cat reports/bench_xxx/2_perf_stat.txt

# 查看熱點函數
cat reports/bench_xxx/3_perf_hotspots.txt

# 查看火焰圖（若已生成）
xdg-open reports/bench_xxx/5_flamegraph.svg

# 重新分析 perf.data
perf report -i reports/bench_xxx/perf.data --hierarchy -M intel

# 重新分析 cachegrind.out
cg_annotate --auto=yes reports/bench_xxx/cachegrind.out
```

## Perf 權限設定

```bash
# 允許非 root 使用 perf
sudo sysctl -w kernel.perf_event_paranoid=-1

# 永久設定
echo "kernel.perf_event_paranoid = -1" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

## 工具依賴

```bash
# Debian/Ubuntu
sudo apt install linux-tools-generic valgrind

# FlameGraph (可選)
git clone https://github.com/brendangregg/FlameGraph.git
export PATH="$PATH:$PWD/FlameGraph"
```

## 報表範例

### 優秀範例 (Production-Ready)
```
════════════════════════════════════════════════════════════
HFT Performance Scorecard
Generated: 20260111_143022
Target: build/release/bench_orderbook
Filter: BM_Add
════════════════════════════════════════════════════════════

[延遲指標 (Latency Metrics)]
BM_OrderBook_Add    45 ns    44 ns    15234567 iterations

[CPU 效率 (CPU Efficiency)]
2.1 insn per cycle

[快取效率 (Cache Efficiency)]
L1-dcache-load-misses     1.42% of all L1-dcache hits

[分支預測 (Branch Prediction)]
branch-misses             0.78% of all branches

[熱點函數 Top 5]
    15.2%  OrderBook::add
    12.3%  PriceLevel::insert
     8.1%  std::lower_bound
     ...
```

### 需優化範例
```
[延遲指標]
BM_OrderBook_Add    320 ns    315 ns    2134567 iterations

[CPU 效率]
1.1 insn per cycle

[快取效率]
L1-dcache-load-misses     12.5% of all L1-dcache hits

[分支預測]
branch-misses             4.2% of all branches
```

**診斷**: IPC 過低 + L1 Miss 過高 → 資料佈局問題（考慮 SoA + alignas）

---

**Remember: 沒有量測過的優化都是妄想。Always Profile.**
