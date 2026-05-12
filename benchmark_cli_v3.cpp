/**
 * benchmark_cli_v3.cpp - AI2ORBIT CMD LINE V3
 * GAUSSIAN CALIBRATION - ACCEPT ONLY SLOW
 *
 * Copyright (c) AI2ORBIT Co. 2026
 * Authors: Sami Leino, Anirudha Talmale
 * All rights reserved.
 *
 * This software is proprietary and confidential.
 * Unauthorized copying, distribution, or modification is strictly prohibited.
 *
 * Methodology:
 *   - 300,000 iterations per column (3 columns: 33/33/33)
 *   - Time ALL attempts in each 300K run
 *   - ACCEPT ONLY SLOWEST results (small number from each run)
 *   - Fit Gaussian to slow results only
 *   - Bandwidth: 10-300 kbps, content in 5-15 seconds
 *   - Phone content indexing for fast server transport
 *   - CPU goes slow then really fast
 */

#include "cpu_benchmark.h"
#include "memory_benchmark.h"
#include "dram_mapper.h"
#include "cuda_benchmark.h"
#include "timer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

static constexpr std::size_t LOOP_COUNT = 300000;
static constexpr int COL_ACCEPT = 33;
static constexpr int NUM_COLUMNS = 3;
static constexpr int TOTAL_SLOW = COL_ACCEPT * NUM_COLUMNS;

static constexpr double BW_MIN_KBPS = 700.0;
static constexpr double BW_MAX_KBPS = 11250.0;
static constexpr double DELIVER_MIN_SEC = 5.0;
static constexpr double DELIVER_MAX_SEC = 15.0;

struct GaussianParams {
    double mean;
    double sigma;
    double amplitude;
};

struct ColumnResult {
    std::vector<double> accepted_slow;
    double all_min_ns;
    double all_max_ns;
    double all_avg_ns;
    double all_median_ns;
    GaussianParams gauss;
    double throughput_kbps;
};

struct TransportProfile {
    double index_rate_ops;
    double content_kb_5sec_min;
    double content_kb_5sec_max;
    double content_kb_15sec_min;
    double content_kb_15sec_max;
    double optimal_chunk_kb;
    double slow_baseline_ms;
};

static unsigned int get_thread_count() {
    unsigned int n = std::thread::hardware_concurrency();
    return (n > 0) ? n : 4;
}

static void enable_ansi() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        GetConsoleMode(hOut, &mode);
        SetConsoleMode(hOut, mode | 0x0004);
    }
#endif
}

static void print_sep() {
    std::cout << "  " << std::string(62, '-') << "\n";
}

static void pm(const std::string& label, double value,
               const std::string& unit, int prec = 2) {
    std::cout << "  " << std::left << std::setw(30) << label
              << std::right << std::fixed << std::setprecision(prec)
              << std::setw(16) << value << " " << unit << "\n";
}

static void pm_sci(const std::string& label, double value,
                    const std::string& unit) {
    std::cout << "  " << std::left << std::setw(30) << label
              << std::right << std::scientific << std::setprecision(6)
              << std::setw(16) << value << " " << unit << "\n";
}

static GaussianParams fit_gaussian(const std::vector<double>& values) {
    GaussianParams gp{};
    if (values.empty()) return gp;

    double sum = 0.0, sum2 = 0.0;
    for (double v : values) {
        sum += v;
        sum2 += v * v;
    }
    double n = static_cast<double>(values.size());
    gp.mean = sum / n;
    double variance = (sum2 / n) - (gp.mean * gp.mean);
    gp.sigma = std::sqrt(std::max(variance, 1e-20));
    gp.amplitude = 1.0 / (gp.sigma * std::sqrt(2.0 * M_PI));

    return gp;
}

// Run 300K iterations, time every attempt, accept only 33 slowest
static ColumnResult run_column(int col_num) {
    ColumnResult cr{};

    std::cout << "    Column " << col_num << ": timing " << LOOP_COUNT
              << " attempts...\n";

    std::vector<double> all_times(LOOP_COUNT);

    for (std::size_t i = 0; i < LOOP_COUNT; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        volatile double result = 0.0;
        double x = static_cast<double>(i) * 0.0001 + 1.0;
        for (int k = 0; k < 10; ++k) {
            x = std::sin(x) * std::cos(x * 0.5) + std::log(x + 1.0);
            x = std::exp(-x * 0.01) * std::sqrt(std::abs(x) + 1.0);
        }
        result = x;
        (void)result;

        auto end = std::chrono::high_resolution_clock::now();
        all_times[i] = std::chrono::duration<double, std::nano>(end - start).count();
    }

    // Sort all 300K results
    std::vector<double> sorted = all_times;
    std::sort(sorted.begin(), sorted.end());

    cr.all_min_ns = sorted.front();
    cr.all_max_ns = sorted.back();
    cr.all_avg_ns = std::accumulate(sorted.begin(), sorted.end(), 0.0) / static_cast<double>(LOOP_COUNT);
    cr.all_median_ns = sorted[LOOP_COUNT / 2];

    // ACCEPT ONLY SLOWEST 33 results from 300K
    cr.accepted_slow.resize(COL_ACCEPT);
    for (int i = 0; i < COL_ACCEPT; ++i) {
        cr.accepted_slow[i] = sorted[LOOP_COUNT - 1 - i];
    }

    // Fit Gaussian to accepted slow results only
    cr.gauss = fit_gaussian(cr.accepted_slow);

    // Throughput based on slow baseline
    cr.throughput_kbps = (10.0 * 8.0 * 1e9) / cr.gauss.mean / 1000.0;

    return cr;
}

static void print_column(const ColumnResult& cr, int col_num) {
    std::cout << "\n    Column " << col_num << " (" << LOOP_COUNT
              << " timed, " << COL_ACCEPT << " slowest accepted):\n";
    print_sep();

    std::cout << "    ALL 300K attempts:\n";
    pm_sci("      Min Time:", cr.all_min_ns, "ns");
    pm_sci("      Max Time:", cr.all_max_ns, "ns");
    pm_sci("      Avg Time:", cr.all_avg_ns, "ns");
    pm_sci("      Median Time:", cr.all_median_ns, "ns");

    std::cout << "\n    ACCEPTED SLOWEST " << COL_ACCEPT << " (from " << LOOP_COUNT << "):\n";
    for (int i = 0; i < std::min(COL_ACCEPT, 10); ++i) {
        std::ostringstream lbl;
        lbl << "      [" << (i + 1) << "]:";
        pm_sci(lbl.str(), cr.accepted_slow[i], "ns");
    }
    if (COL_ACCEPT > 10)
        std::cout << "      ... (" << (COL_ACCEPT - 10) << " more)\n";

    std::cout << "\n    GAUSSIAN (slow only):\n";
    pm_sci("      Mean:", cr.gauss.mean, "ns");
    pm_sci("      Sigma:", cr.gauss.sigma, "ns");
    pm_sci("      Mean:", cr.gauss.mean * 1000.0, "ps");
    pm_sci("      Mean:", cr.gauss.mean / 1e6, "ms");
    pm("      Throughput:", cr.throughput_kbps, "kbps", 2);
    print_sep();
}

// Screen blink for GPU
struct ScreenBlinkResult {
    double frames;
    double fps;
    double time_sec;
};

static ScreenBlinkResult run_screen_blink(double duration = 3.0) {
    ScreenBlinkResult r{};
    const int w = 80, h = 5;
    struct CD { int bg; int fg; const char* lbl; };
    CD colors[] = {
        {47,30,"WHITE"},{42,30,"GREEN"},{46,30,"CYAN"},{43,30,"YELLOW"},
        {45,37,"MAGENTA"},{41,37,"RED"},{44,37,"BLUE"},{40,97,"BLACK"},
        {107,30,"BRIGHT W"},{102,30,"BRIGHT G"},{106,30,"BRIGHT C"},{103,30,"BRIGHT Y"},
    };
    Timer t; t.start();
    double frames = 0; int ci = 0;
    while (t.elapsed_seconds() < duration) {
        std::cout << "\033[" << colors[ci%12].bg << ";" << colors[ci%12].fg << "m";
        for (int row = 0; row < h; ++row) {
            std::string line(w, ' ');
            if (row == h/2) {
                std::string lbl = colors[ci%12].lbl;
                int pad = (w - (int)lbl.size())/2;
                if (pad > 0) for (size_t i=0;i<lbl.size();++i) line[pad+i]=lbl[i];
            } else {
                int pos = (int)frames % w;
                for (int c=0;c<w;++c) { int d=std::abs(c-pos); if(d<3)line[c]='#'; else if(d<6)line[c]='='; }
            }
            std::cout << line << "\n";
        }
        std::cout << "\033[0m"; std::cout.flush();
        frames++; ci++;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        std::cout << "\033[" << h << "A";
    }
    for (int i=0;i<h;++i) std::cout << "\033[0m" << std::string(w,' ') << "\n";
    std::cout << "\033[" << h << "A\033[0m"; std::cout.flush();
    r.time_sec = t.elapsed_seconds(); r.frames = frames; r.fps = frames/r.time_sec;
    return r;
}

int main(int argc, char* argv[]) {
    enable_ansi();

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "    ___   ____  ___   ____  ____  ____  ______\n";
    std::cout << "   /   | /  _/ / _ \\ / __ \\/ __ )/ / / /_  __/\n";
    std::cout << "  / /| | / /  / /_/ // /_/ / __ / / / / / /\n";
    std::cout << " / ___ |/ /  /  ___// _, _/ /_/ / /_/ / / /\n";
    std::cout << "/_/  |_/___/ /_/   /_/ |_/_____/\\____/ /_/\n";
    std::cout << "\n";
    std::cout << "  BenchmarkCore CMD LINE V3\n";
    std::cout << "  GAUSSIAN CALIBRATION - ACCEPT ONLY SLOW\n";
    std::cout << "  Copyright (c) AI2ORBIT Co. 2026\n";
    std::cout << "  Authors: Sami Leino, Anirudha Talmale\n";
    std::cout << "  Platform: Windows 11 x86_64\n";
    std::cout << "  CPU Threads: " << get_thread_count() << "\n";
    std::cout << "================================================================\n\n";

    std::cout << "  METHODOLOGY:\n";
    print_sep();
    std::cout << "  Columns: 3 (equal 33/33/33)\n";
    std::cout << "  Attempts per column: 300,000 (all timed)\n";
    std::cout << "  Accept: ONLY slowest 33 from each 300K run\n";
    std::cout << "  Total accepted: " << TOTAL_SLOW << " slow results\n";
    std::cout << "  Gaussian: fit to slow results only\n";
    std::cout << "  CPU behavior: goes SLOW then REALLY FAST\n";
    std::cout << "  Bandwidth: 700-11250 kbps\n";
    std::cout << "  Content delivery: 5-15 seconds\n";
    std::cout << "  Phone content indexing for fast server transport\n";
    print_sep();

    auto total_start = std::chrono::high_resolution_clock::now();

    // Run 3 columns, accept only slowest 33 from each
    ColumnResult cols[NUM_COLUMNS];

    for (int c = 0; c < NUM_COLUMNS; ++c) {
        std::cout << "\n";
        std::cout << "================================================================\n";
        std::cout << "  COLUMN " << (c+1) << " / " << NUM_COLUMNS
                  << " (300K attempts -> accept slowest " << COL_ACCEPT << ")\n";
        std::cout << "================================================================\n";

        cols[c] = run_column(c + 1);
        print_column(cols[c], c + 1);
    }

    // ================================================================
    // COMBINED GAUSSIAN: all 99 slow results together
    // ================================================================
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  COMBINED GAUSSIAN (all " << TOTAL_SLOW << " slow results)\n";
    std::cout << "  Accept ONLY slow. CPU goes slow then really fast.\n";
    std::cout << "================================================================\n\n";

    std::vector<double> all_slow;
    all_slow.reserve(TOTAL_SLOW);
    for (int c = 0; c < NUM_COLUMNS; ++c) {
        for (double v : cols[c].accepted_slow) {
            all_slow.push_back(v);
        }
    }

    GaussianParams combined = fit_gaussian(all_slow);

    std::cout << "  GAUSSIAN FIT (99 slowest from 900K total attempts):\n";
    print_sep();
    pm_sci("  Mean:", combined.mean, "ns");
    pm_sci("  Sigma:", combined.sigma, "ns");
    pm_sci("  Amplitude:", combined.amplitude, "");
    pm_sci("  Mean:", combined.mean * 1000.0, "ps");
    pm_sci("  Mean:", combined.mean / 1e6, "ms");
    print_sep();

    double slow_kbps = (10.0 * 8.0 * 1e9) / combined.mean / 1000.0;
    double fast_avg = (cols[0].all_min_ns + cols[1].all_min_ns + cols[2].all_min_ns) / 3.0;
    double accel_factor = combined.mean / fast_avg;

    std::cout << "\n  SLOW -> FAST PROFILE:\n";
    print_sep();
    pm_sci("  Slow Baseline (accepted):", combined.mean, "ns");
    pm_sci("  Fast Peak (min of 300K):", fast_avg, "ns");
    pm("  Acceleration:", accel_factor, "x", 1);
    pm("  Slow Throughput:", slow_kbps, "kbps", 2);
    double fast_kbps = (10.0 * 8.0 * 1e9) / fast_avg / 1000.0;
    pm("  Fast Throughput:", fast_kbps, "kbps", 2);
    pm("  Speedup:", accel_factor * 100.0, "%", 0);
    print_sep();

    // ================================================================
    // GPU + SCREEN BLINK
    // ================================================================
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  GPU PROCESSING + SCREEN BLINK (~3 sec)\n";
    std::cout << "================================================================\n";

    unsigned int num_threads = get_thread_count();
    std::cout << "\n  >>> SCREEN BLINK <<<\n\n";

    ScreenBlinkResult blink{};
    std::vector<CudaBenchmark::Results> gpu_results(num_threads);
    std::vector<std::thread> threads;

    Timer t;
    t.start();

    std::thread blink_thread([&blink]() { blink = run_screen_blink(3.0); });
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&gpu_results, i]() {
            CudaBenchmark gpu;
            gpu_results[i] = gpu.run(8 * 1024 * 1024);
        });
    }
    blink_thread.join();
    for (auto& th : threads) th.join();

    double gpu_elapsed = t.elapsed_seconds();
    double total_gflops = 0.0;
    for (unsigned int i = 0; i < num_threads; ++i)
        total_gflops += gpu_results[i].compute_gflops;

    std::cout << "\n  >>> BLINK COMPLETE <<<\n\n";
    pm("  GPU Compute:", total_gflops, "GFLOPS", 2);
    pm("  Blink Frames:", blink.frames, "", 0);
    pm("  Blink FPS:", blink.fps, "FPS", 1);
    pm("  Time:", gpu_elapsed, "sec", 4);
    print_sep();

    // ================================================================
    // DRAM BENCHMARK
    // ================================================================
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  DRAM MEASUREMENT\n";
    std::cout << "================================================================\n";

    MemoryBenchmark mem_bench;
    auto mem_result = mem_bench.run(16 * 1024 * 1024, 100);
    double dram_mbps = mem_result.throughput_mbps;
    double dram_latency_ns = mem_result.timing.avg_latency_ns;

    std::cout << "\n";
    pm("  DRAM Bandwidth:", dram_mbps, "MB/s", 2);
    pm("  DRAM Latency:", dram_latency_ns, "ns", 2);
    print_sep();

    // ================================================================
    // RF UNIT SIMULATION
    // ================================================================
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  RF UNIT (radio frequency transport simulation)\n";
    std::cout << "================================================================\n";

    double rf_score = 0.0;
    {
        Timer rf_timer;
        rf_timer.start();
        volatile double rf_acc = 0.0;
        for (int i = 0; i < 500000; ++i) {
            double freq = 2.4e9 + static_cast<double>(i) * 1000.0;
            double wavelength = 3e8 / freq;
            double signal = std::sin(freq * 1e-9) * std::exp(-wavelength * 0.1);
            double modulated = signal * std::cos(freq * 0.5e-9);
            rf_acc += modulated;
        }
        (void)rf_acc;
        double rf_elapsed = rf_timer.elapsed_seconds();
        rf_score = 500000.0 / rf_elapsed;
    }

    pm("  RF Throughput:", rf_score, "signals/sec", 0);
    pm("  RF Score:", rf_score / 1e6, "M", 2);
    print_sep();

    // ================================================================
    // POWER CHAIN: cpu^dram^cpu^gpu^screen^cpu^rfunit
    // Exponents: 2, 3, 1, 4
    // ================================================================
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  POWER CHAIN: cpu^dram^cpu^gpu^screen^cpu^rfunit\n";
    std::cout << "  Exponents: 2, 3, 1, 4\n";
    std::cout << "================================================================\n\n";

    double cpu_score = accel_factor;
    double dram_score_norm = dram_mbps / 100.0;
    double gpu_score_norm = total_gflops / 100.0;
    double screen_score = blink.fps;
    double rf_score_norm = rf_score / 1e6;

    double p_cpu   = std::pow(cpu_score, 2.0);
    double p_dram  = std::pow(dram_score_norm, 3.0);
    double p_gpu   = std::pow(gpu_score_norm, 1.0);
    double p_screen = std::pow(screen_score / 10.0, 1.0);
    double p_rf    = std::pow(rf_score_norm, 4.0);

    double power_factor = p_cpu * p_dram * p_gpu * p_screen * p_rf;

    std::cout << "  COMPONENT SCORES:\n";
    print_sep();
    pm("  CPU (accel):", cpu_score, "x", 1);
    pm("  DRAM (norm):", dram_score_norm, "", 2);
    pm("  GPU (norm):", gpu_score_norm, "", 2);
    pm("  Screen (FPS):", screen_score, "FPS", 1);
    pm("  RF (M signals):", rf_score_norm, "M", 2);
    print_sep();

    std::cout << "\n  POWER CHAIN (exponents 2,3,1,4):\n";
    print_sep();
    pm("  CPU^2:", p_cpu, "", 2);
    pm("  DRAM^3:", p_dram, "", 2);
    pm("  GPU^1:", p_gpu, "", 2);
    pm("  Screen^1:", p_screen, "", 2);
    pm("  RF^4:", p_rf, "", 2);
    pm_sci("  Power Factor:", power_factor, "");
    print_sep();

    double boosted_slow_kbps = slow_kbps * power_factor;
    double boosted_fast_kbps = fast_kbps * power_factor;

    std::cout << "\n  POWERED THROUGHPUT:\n";
    print_sep();
    pm("  Base Slow:", slow_kbps, "kbps", 2);
    pm_sci("  Powered Slow:", boosted_slow_kbps, "kbps");
    pm("  Base Fast:", fast_kbps, "kbps", 2);
    pm_sci("  Powered Fast:", boosted_fast_kbps, "kbps");
    pm_sci("  Power Factor:", power_factor, "x");
    print_sep();

    // ================================================================
    // TRANSPORT: phone content indexing with power boost
    // ================================================================
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  PHONE CONTENT INDEXING - POWERED TRANSPORT\n";
    std::cout << "  Bandwidth: 700-11250 kbps | Delivery: 5-15 sec\n";
    std::cout << "  Power: cpu^dram^cpu^gpu^screen^cpu^rfunit\n";
    std::cout << "================================================================\n\n";

    TransportProfile tp{};
    tp.slow_baseline_ms = combined.mean / 1e6;
    tp.index_rate_ops = (1e9 / combined.mean) * power_factor;
    tp.content_kb_5sec_min = BW_MIN_KBPS * DELIVER_MIN_SEC * power_factor;
    tp.content_kb_5sec_max = BW_MAX_KBPS * DELIVER_MIN_SEC * power_factor;
    tp.content_kb_15sec_min = BW_MIN_KBPS * DELIVER_MAX_SEC * power_factor;
    tp.content_kb_15sec_max = BW_MAX_KBPS * DELIVER_MAX_SEC * power_factor;

    double mid_bw = (BW_MIN_KBPS + BW_MAX_KBPS) / 2.0;
    double mid_time = (DELIVER_MIN_SEC + DELIVER_MAX_SEC) / 2.0;
    tp.optimal_chunk_kb = mid_bw * mid_time * power_factor / accel_factor;

    std::cout << "  INDEX PERFORMANCE (powered):\n";
    print_sep();
    pm("  Slow Baseline:", tp.slow_baseline_ms, "ms", 6);
    pm_sci("  Index Rate:", tp.index_rate_ops, "ops/sec");
    pm("  GPU Assist:", total_gflops, "GFLOPS", 2);
    pm_sci("  Power Factor:", power_factor, "x");
    print_sep();

    std::cout << "\n  TRANSPORT CAPACITY (powered):\n";
    print_sep();
    pm_sci("  5 sec @ 700 kbps:", tp.content_kb_5sec_min, "KB");
    pm_sci("  5 sec @ 11250 kbps:", tp.content_kb_5sec_max, "KB");
    pm_sci("  15 sec @ 700 kbps:", tp.content_kb_15sec_min, "KB");
    pm_sci("  15 sec @ 11250 kbps:", tp.content_kb_15sec_max, "KB");
    pm_sci("  Optimal Chunk:", tp.optimal_chunk_kb, "KB");
    print_sep();

    // ================================================================
    // FINAL SCOREBOARD
    // ================================================================
    auto total_end = std::chrono::high_resolution_clock::now();
    double total_seconds = std::chrono::duration<double>(total_end - total_start).count();

    std::cout << "\n\n";
    std::cout << "================================================================\n";
    std::cout << "  FINAL RESULTS - ACCEPT ONLY SLOW\n";
    std::cout << "================================================================\n\n";

    std::cout << "  COLUMN SUMMARY:\n";
    print_sep();
    std::cout << "  Col  Attempts  Accepted  SlowMean(ns)     SlowSigma(ns)\n";
    std::cout << "  ---  --------  --------  ---------------  ---------------\n";
    for (int c = 0; c < NUM_COLUMNS; ++c) {
        std::cout << "  " << std::setw(3) << (c+1)
                  << "  " << std::setw(8) << LOOP_COUNT
                  << "  " << std::setw(8) << COL_ACCEPT
                  << "  " << std::scientific << std::setprecision(6)
                  << std::setw(15) << cols[c].gauss.mean
                  << "  " << std::setw(15) << cols[c].gauss.sigma
                  << "\n";
    }
    std::cout << "  ---  --------  --------  ---------------  ---------------\n";
    std::cout << "  Tot  " << std::setw(8) << (LOOP_COUNT * NUM_COLUMNS)
              << "  " << std::setw(8) << TOTAL_SLOW << "\n\n";

    std::cout << "  COMBINED GAUSSIAN (slow only):\n";
    print_sep();
    pm_sci("  Mean:", combined.mean, "ns");
    pm_sci("  Sigma:", combined.sigma, "ns");
    pm("  Throughput (slow):", slow_kbps, "kbps", 2);
    pm("  Throughput (fast):", fast_kbps, "kbps", 2);
    pm("  Acceleration:", accel_factor, "x", 1);
    pm("  Speedup:", accel_factor * 100.0, "%", 0);
    pm("  GPU:", total_gflops, "GFLOPS", 2);
    pm("  DRAM:", dram_mbps, "MB/s", 2);
    pm("  RF:", rf_score / 1e6, "M signals/sec", 2);
    print_sep();

    std::cout << "\n  POWER CHAIN RESULT:\n";
    print_sep();
    std::cout << "  cpu^2 * dram^3 * gpu^1 * screen^1 * rf^4\n";
    pm_sci("  Power Factor:", power_factor, "x");
    pm_sci("  Powered Throughput:", boosted_slow_kbps, "kbps");
    print_sep();

    std::cout << "\n";
    pm("  Total Benchmark Time:", total_seconds, "seconds", 2);
    std::cout << "\n  Calibration complete.\n";
    std::cout << "  900K attempts timed, " << TOTAL_SLOW << " slowest accepted.\n";
    std::cout << "  Power: cpu^dram^cpu^gpu^screen^cpu^rfunit (2,3,1,4)\n";
    std::cout << "  CPU: slow baseline -> really fast.\n";

    std::cout << "\n  Platform: Windows 11 x86_64\n";
    std::cout << "================================================================\n";
    std::cout << "  Copyright (c) AI2ORBIT Co. 2026. All rights reserved.\n";
    std::cout << "================================================================\n\n";

    return 0;
}
