/**
 * benchmark_cli.cpp - AI2ORBIT Windows CLI Benchmark Tool
 *
 * Copyright (c) AI2ORBIT Co. 2026
 * Authors: Sami Leino, Anirudha Talmale
 * All rights reserved.
 *
 * This software is proprietary and confidential.
 * Unauthorized copying, distribution, or modification is strictly prohibited.
 *
 * Runs CPU, DRAM, and GPU benchmarks in slow and fast phases.
 * Phasing speed: (46.7/1000 | 47.3/1000)/4
 * Designed to stress hardware and display all numbers for recording.
 */

#include "cpu_benchmark.h"
#include "memory_benchmark.h"
#include "dram_mapper.h"
#include "cuda_benchmark.h"
#include "timer.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static constexpr double PHASE_SLOW = 46.7 / 1000.0;
static constexpr double PHASE_FAST = 47.3 / 1000.0;
static constexpr double PHASE_DIVISOR = 4.0;
static constexpr double PHASE_SLOW_RATE = PHASE_SLOW / PHASE_DIVISOR;
static constexpr double PHASE_FAST_RATE = PHASE_FAST / PHASE_DIVISOR;

struct PhaseResult {
    std::string name;
    std::string component;
    double value;
    std::string unit;
    double time_seconds;
    double phasing_speed;
};

static void print_banner() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "    ___   ____  ___   ____  ____  ____  ______\n";
    std::cout << "   /   | /  _/ / _ \\ / __ \\/ __ )/ / / /_  __/\n";
    std::cout << "  / /| | / /  / /_/ // /_/ / __ / / / / / /\n";
    std::cout << " / ___ |/ /  /  ___// _, _/ /_/ / /_/ / / /\n";
    std::cout << "/_/  |_/___/ /_/   /_/ |_/_____/\\____/ /_/\n";
    std::cout << "\n";
    std::cout << "  BenchmarkCore CLI v2.0.0 - Hardware Stress Test\n";
    std::cout << "  Copyright (c) AI2ORBIT Co. 2026\n";
    std::cout << "  Authors: Sami Leino, Anirudha Talmale\n";
    std::cout << "  Platform: Windows 11 x86_64\n";
    std::cout << "================================================================\n\n";

    std::cout << "  Phasing Speed Formula: (46.7/1000 | 47.3/1000) / 4\n";
    std::cout << "  Slow Phase Rate: " << std::fixed << std::setprecision(6)
              << PHASE_SLOW_RATE << " GHz\n";
    std::cout << "  Fast Phase Rate: " << std::fixed << std::setprecision(6)
              << PHASE_FAST_RATE << " GHz\n\n";
}

static void print_phase_header(int phase_num, int total, const std::string& name,
                                const std::string& speed_label) {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  PHASE " << phase_num << "/" << total << " - " << name
              << " [" << speed_label << "]\n";
    std::cout << "================================================================\n";
}

static void print_separator() {
    std::cout << "  " << std::string(56, '-') << "\n";
}

static void print_metric(const std::string& label, double value,
                          const std::string& unit, int precision = 2) {
    std::cout << "  " << std::left << std::setw(28) << label
              << std::right << std::fixed << std::setprecision(precision)
              << std::setw(14) << value << " " << unit << "\n";
}

static void print_metric_str(const std::string& label, const std::string& value) {
    std::cout << "  " << std::left << std::setw(28) << label
              << std::right << std::setw(14) << value << "\n";
}

int main(int argc, char* argv[]) {
    print_banner();

    auto total_start = std::chrono::high_resolution_clock::now();
    std::vector<PhaseResult> all_results;
    const int TOTAL_PHASES = 6;

    // ================================================================
    // PHASE 1: CPU PROCESSING - SLOW
    // ================================================================
    {
        print_phase_header(1, TOTAL_PHASES, "CPU PROCESSING", "SLOW");
        std::cout << "  Warming up CPU cores...\n";
        std::cout << "  Iterations: 500,000 (Integer + Float + Memory workload)\n";
        print_separator();

        CpuBenchmark cpu;
        Timer t;
        t.start();
        auto r = cpu.run(500000);
        double elapsed = t.elapsed_seconds();

        double mflops = r.timing.operations_per_second / 1e6;
        double phasing = mflops * PHASE_SLOW_RATE;

        print_metric("Total Time:", elapsed, "sec", 4);
        print_metric("Operations/sec:", r.timing.operations_per_second, "ops/s", 0);
        print_metric("Throughput:", mflops, "MFlops");
        print_metric("Time/Operation:", r.timing.time_per_operation_ns, "ns", 2);
        print_metric("Phasing Speed:", phasing, "MFlops*GHz", 4);
        print_separator();
        print_metric_str("Status:", r.benchmark_successful ? "PASS" : "FAIL");

        PhaseResult pr;
        pr.name = "CPU SLOW";
        pr.component = "CPU";
        pr.value = mflops;
        pr.unit = "MFlops";
        pr.time_seconds = elapsed;
        pr.phasing_speed = phasing;
        all_results.push_back(pr);
    }

    // ================================================================
    // PHASE 2: CPU PROCESSING - FAST
    // ================================================================
    {
        print_phase_header(2, TOTAL_PHASES, "CPU PROCESSING", "FAST");
        std::cout << "  Full CPU stress - all execution units engaged...\n";
        std::cout << "  Iterations: 10,000,000 (Integer + Float + Memory workload)\n";
        print_separator();

        CpuBenchmark cpu;
        Timer t;
        t.start();
        auto r = cpu.run(10000000);
        double elapsed = t.elapsed_seconds();

        double mflops = r.timing.operations_per_second / 1e6;
        double phasing = mflops * PHASE_FAST_RATE;

        print_metric("Total Time:", elapsed, "sec", 4);
        print_metric("Operations/sec:", r.timing.operations_per_second, "ops/s", 0);
        print_metric("Throughput:", mflops, "MFlops");
        print_metric("Time/Operation:", r.timing.time_per_operation_ns, "ns", 2);
        print_metric("Phasing Speed:", phasing, "MFlops*GHz", 4);
        print_separator();
        print_metric_str("Status:", r.benchmark_successful ? "PASS" : "FAIL");

        PhaseResult pr;
        pr.name = "CPU FAST";
        pr.component = "CPU";
        pr.value = mflops;
        pr.unit = "MFlops";
        pr.time_seconds = elapsed;
        pr.phasing_speed = phasing;
        all_results.push_back(pr);
    }

    // ================================================================
    // PHASE 3: DRAM PROCESSING - SLOW
    // ================================================================
    {
        print_phase_header(3, TOTAL_PHASES, "DRAM PROCESSING", "SLOW");
        std::cout << "  Probing memory hierarchy (32 MB buffer, 5 cycles)...\n";
        std::cout << "  Read-Write-Verify with latency measurement\n";
        print_separator();

        MemoryBenchmark mem;
        Timer t;
        t.start();
        auto r = mem.run(32 * 1024 * 1024, 5);
        double elapsed = t.elapsed_seconds();

        double phasing = r.throughput_mbps * PHASE_SLOW_RATE;

        print_metric("Total Time:", elapsed, "sec", 4);
        print_metric("Throughput:", r.throughput_mbps, "MB/s", 1);
        print_metric("Min Latency:", r.timing.min_latency_ns, "ns", 0);
        print_metric("Max Latency:", r.timing.max_latency_ns, "ns", 0);
        print_metric("Avg Latency:", r.timing.avg_latency_ns, "ns", 0);
        print_metric("Std Deviation:", r.timing.std_deviation_ns, "ns", 2);
        print_metric("Phasing Speed:", phasing, "MB/s*GHz", 4);
        print_separator();
        print_metric_str("Verification:", r.verification_passed ? "PASS" : "FAIL");

        DramMapper dm;
        auto dm_r = dm.map_memory(32 * 1024 * 1024);
        std::cout << "\n  DRAM Mapper (5-Table Priority):\n";
        print_metric("  Regions Mapped:", static_cast<double>(dm_r.regions_mapped), "", 0);
        print_metric("  Fastest Latency:", dm_r.fastest_latency_ns, "ns", 2);
        print_metric("  Slowest Latency:", dm_r.slowest_latency_ns, "ns", 2);
        print_metric("  Acceleration:", dm_r.acceleration_factor, "x", 2);
        print_metric("  STOP Markers:", static_cast<double>(dm_r.stop_markers_hit), "", 0);
        std::cout << "  Table 1 (CPU RAM):  " << dm_r.tables.table1_cpu_ram.size() << " regions\n";
        std::cout << "  Table 2 (RAM):      " << dm_r.tables.table2_ram.size() << " regions\n";
        std::cout << "  Table 3 (Priority): " << dm_r.tables.table3_priority.size() << " regions\n";
        std::cout << "  Table 4 (Extra):    " << dm_r.tables.table4_extra.size() << " regions\n";
        std::cout << "  Table 5 (Partial):  " << dm_r.tables.table5_partial.size() << " regions\n";

        PhaseResult pr;
        pr.name = "DRAM SLOW";
        pr.component = "DRAM";
        pr.value = r.throughput_mbps;
        pr.unit = "MB/s";
        pr.time_seconds = elapsed;
        pr.phasing_speed = phasing;
        all_results.push_back(pr);
    }

    // ================================================================
    // PHASE 4: DRAM PROCESSING - FAST
    // ================================================================
    {
        print_phase_header(4, TOTAL_PHASES, "DRAM PROCESSING", "FAST");
        std::cout << "  Full DRAM stress (128 MB buffer, 15 cycles)...\n";
        std::cout << "  Maximum memory bus utilization\n";
        print_separator();

        MemoryBenchmark mem;
        Timer t;
        t.start();
        auto r = mem.run(128 * 1024 * 1024, 15);
        double elapsed = t.elapsed_seconds();

        double phasing = r.throughput_mbps * PHASE_FAST_RATE;

        print_metric("Total Time:", elapsed, "sec", 4);
        print_metric("Throughput:", r.throughput_mbps, "MB/s", 1);
        print_metric("Min Latency:", r.timing.min_latency_ns, "ns", 0);
        print_metric("Max Latency:", r.timing.max_latency_ns, "ns", 0);
        print_metric("Avg Latency:", r.timing.avg_latency_ns, "ns", 0);
        print_metric("Std Deviation:", r.timing.std_deviation_ns, "ns", 2);
        print_metric("Phasing Speed:", phasing, "MB/s*GHz", 4);
        print_separator();
        print_metric_str("Verification:", r.verification_passed ? "PASS" : "FAIL");

        DramMapper dm;
        auto dm_r = dm.map_memory(128 * 1024 * 1024);
        double accel = dm.measure_acceleration();
        std::cout << "\n  DRAM Mapper (Full Probe):\n";
        print_metric("  Regions Mapped:", static_cast<double>(dm_r.regions_mapped), "", 0);
        print_metric("  Total Mapped:", static_cast<double>(dm_r.total_mapped_bytes) / (1024.0 * 1024.0), "MB", 1);
        print_metric("  Virtual Pool:", static_cast<double>(dm_r.virtual_pool_bytes) / (1024.0 * 1024.0), "MB", 1);
        print_metric("  Virt Ratio:", dm_r.virtualization_ratio, "x", 1);
        print_metric("  Acceleration:", dm_r.acceleration_factor, "x", 2);
        print_metric("  Measured Accel:", accel, "x", 2);

        PhaseResult pr;
        pr.name = "DRAM FAST";
        pr.component = "DRAM";
        pr.value = r.throughput_mbps;
        pr.unit = "MB/s";
        pr.time_seconds = elapsed;
        pr.phasing_speed = phasing;
        all_results.push_back(pr);
    }

    // ================================================================
    // PHASE 5: GPU PROCESSING - SLOW
    // ================================================================
    {
        print_phase_header(5, TOTAL_PHASES, "GPU PROCESSING", "SLOW");
        std::cout << "  GPU compute warmup (256K elements)...\n";
        std::cout << "  Matrix multiply + memory bandwidth + PCIe transfer\n";
        print_separator();

        CudaBenchmark gpu;
        Timer t;
        t.start();
        auto r = gpu.run(256 * 1024);
        double elapsed = t.elapsed_seconds();

        double phasing = r.compute_gflops * PHASE_SLOW_RATE;

        print_metric("Total Time:", elapsed, "sec", 4);
        print_metric_str("Device:", r.device_name);
        print_metric("Compute:", r.compute_gflops, "GFLOPS", 2);
        print_metric("Memory BW:", r.memory_bandwidth_gbps, "GB/s", 2);
        print_metric("CPU->GPU Transfer:", r.cpu_to_gpu_transfer_gbps, "GB/s", 2);
        print_metric("GPU->CPU Transfer:", r.gpu_to_cpu_transfer_gbps, "GB/s", 2);
        print_metric("Phasing Speed:", phasing, "GFLOPS*GHz", 4);
        print_separator();
        print_metric_str("Status:", r.benchmark_successful ? "PASS" : "FAIL");

        PhaseResult pr;
        pr.name = "GPU SLOW";
        pr.component = "GPU";
        pr.value = r.compute_gflops;
        pr.unit = "GFLOPS";
        pr.time_seconds = elapsed;
        pr.phasing_speed = phasing;
        all_results.push_back(pr);
    }

    // ================================================================
    // PHASE 6: GPU PROCESSING - FAST
    // ================================================================
    {
        print_phase_header(6, TOTAL_PHASES, "GPU PROCESSING", "FAST");
        std::cout << "  Full GPU stress (4M elements)...\n";
        std::cout << "  Maximum compute + bandwidth + transfer\n";
        print_separator();

        CudaBenchmark gpu;
        Timer t;
        t.start();
        auto r = gpu.run(4 * 1024 * 1024);
        double elapsed = t.elapsed_seconds();

        double phasing = r.compute_gflops * PHASE_FAST_RATE;

        print_metric("Total Time:", elapsed, "sec", 4);
        print_metric_str("Device:", r.device_name);
        print_metric("Compute:", r.compute_gflops, "GFLOPS", 2);
        print_metric("Memory BW:", r.memory_bandwidth_gbps, "GB/s", 2);
        print_metric("CPU->GPU Transfer:", r.cpu_to_gpu_transfer_gbps, "GB/s", 2);
        print_metric("GPU->CPU Transfer:", r.gpu_to_cpu_transfer_gbps, "GB/s", 2);
        print_metric("Phasing Speed:", phasing, "GFLOPS*GHz", 4);
        print_separator();
        print_metric_str("Status:", r.benchmark_successful ? "PASS" : "FAIL");

        PhaseResult pr;
        pr.name = "GPU FAST";
        pr.component = "GPU";
        pr.value = r.compute_gflops;
        pr.unit = "GFLOPS";
        pr.time_seconds = elapsed;
        pr.phasing_speed = phasing;
        all_results.push_back(pr);
    }

    // ================================================================
    // FINAL SCOREBOARD
    // ================================================================
    auto total_end = std::chrono::high_resolution_clock::now();
    double total_seconds = std::chrono::duration<double>(total_end - total_start).count();

    std::cout << "\n\n";
    std::cout << "================================================================\n";
    std::cout << "  AI2ORBIT BenchmarkCore v2.0.0 - FULL RESULTS\n";
    std::cout << "================================================================\n\n";

    std::cout << "  Phasing Formula: (46.7/1000 | 47.3/1000) / 4\n";
    std::cout << "  Slow Rate: " << std::fixed << std::setprecision(6) << PHASE_SLOW_RATE << " GHz\n";
    std::cout << "  Fast Rate: " << std::fixed << std::setprecision(6) << PHASE_FAST_RATE << " GHz\n\n";

    std::cout << "  Phase              Value          Unit         Time(s)   Phasing\n";
    std::cout << "  -----------------  -------------  -----------  --------  ----------\n";

    double total_phasing = 0.0;
    double total_phase_time = 0.0;

    for (const auto& pr : all_results) {
        std::ostringstream val_s, time_s, phase_s;
        val_s << std::fixed << std::setprecision(2) << pr.value;
        time_s << std::fixed << std::setprecision(3) << pr.time_seconds;
        phase_s << std::fixed << std::setprecision(4) << pr.phasing_speed;

        std::cout << "  " << std::left << std::setw(19) << pr.name
                  << std::right << std::setw(13) << val_s.str()
                  << "  " << std::left << std::setw(11) << pr.unit
                  << "  " << std::right << std::setw(8) << time_s.str()
                  << "  " << std::right << std::setw(10) << phase_s.str()
                  << "\n";

        total_phasing += pr.phasing_speed;
        total_phase_time += pr.time_seconds;
    }

    std::cout << "  -----------------  -------------  -----------  --------  ----------\n\n";

    double combined_slow = (all_results[0].phasing_speed +
                            all_results[2].phasing_speed +
                            all_results[4].phasing_speed);
    double combined_fast = (all_results[1].phasing_speed +
                            all_results[3].phasing_speed +
                            all_results[5].phasing_speed);
    double full_speed = total_phasing / PHASE_DIVISOR;

    std::cout << "  COMBINED RESULTS:\n";
    std::cout << "  " << std::string(56, '-') << "\n";
    print_metric("Combined Slow Phasing:", combined_slow, "", 4);
    print_metric("Combined Fast Phasing:", combined_fast, "", 4);
    print_metric("Total Phasing Sum:", total_phasing, "", 4);
    print_metric("Full Speed (sum/4):", full_speed, "GHz-equiv", 4);
    print_metric("Total Bench Time:", total_seconds, "seconds", 2);
    print_metric("Phase Time Sum:", total_phase_time, "seconds", 2);
    std::cout << "  " << std::string(56, '-') << "\n\n";

    double cpu_speedup = 0.0;
    if (all_results[0].value > 0)
        cpu_speedup = all_results[1].value / all_results[0].value;
    double dram_speedup = 0.0;
    if (all_results[2].value > 0)
        dram_speedup = all_results[3].value / all_results[2].value;
    double gpu_speedup = 0.0;
    if (all_results[4].value > 0)
        gpu_speedup = all_results[5].value / all_results[4].value;

    std::cout << "  SPEED RATIOS (Fast / Slow):\n";
    std::cout << "  " << std::string(56, '-') << "\n";
    print_metric("CPU  (Fast/Slow):", cpu_speedup, "x", 3);
    print_metric("DRAM (Fast/Slow):", dram_speedup, "x", 3);
    print_metric("GPU  (Fast/Slow):", gpu_speedup, "x", 3);
    std::cout << "  " << std::string(56, '-') << "\n\n";

    std::cout << "  Platform: Windows 11 x86_64\n";
    std::cout << "  Build: x86_64-w64-mingw32-g++ -O2 -std=c++17\n";
    std::cout << "================================================================\n";
    std::cout << "  Copyright (c) AI2ORBIT Co. 2026. All rights reserved.\n";
    std::cout << "================================================================\n\n";

    return 0;
}
