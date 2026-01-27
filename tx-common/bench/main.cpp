#include <benchmark/benchmark.h>

#include "tx/sys/tsc_timer.hpp"
#include "util.hpp"

int main(int argc, char** argv) {
  tx::sys::TSCTimer::calibrate();

  benchmark::Initialize(&argc, argv);

  LatencyBenchmarkReporter reporter;
  benchmark::RunSpecifiedBenchmarks(&reporter);

  return 0;
}
