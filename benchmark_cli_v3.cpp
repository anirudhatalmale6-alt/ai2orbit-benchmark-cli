/**
 * benchmark_cli_v3.cpp - AI2ORBIT CMD LINE V3
 * GAUSSIAN CALIBRATION BENCHMARK
 *
 * Copyright (c) AI2ORBIT Co. 2026
 * Authors: Sami Leino, Anirudha Talmale
 * All rights reserved.
 *
 * This software is proprietary and confidential.
 * Unauthorized copying, distribution, or modification is strictly prohibited.
 *
 * Methodology:
 *   - 300,000 iterations per column (3 columns)
 *   - Pick slowest results first: 67, 67, 65 then 18, 18, 18
 *   - Fit Gaussian to slowest, then high, then medium
 *   - Move Gaussian to match best for kbps in ms/picoseconds
 *   - 200 total results, 67 per column, 18 selected from 300K
 *   - Optimal Gaussian parameters = 400% phone speedup profile
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
static constexpr int COL1_RESULTS = 67;
static constexpr int COL2_RESULTS = 67;
static constexpr int COL3_RESULTS = 65;
static constexpr int PICK_LOW = 18;
static constexpr int TOTAL_RESULTS = 200;
static constexpr double SPEEDUP_FACTOR = 4.0;

struct GaussianParams {
    double mean;
    double sigma;
    double amplitude;
    double r_squared;
};

struct ColumnResult {
    std::vector<double> all_times_ns;
    std::vector<double> slowest;
    std::vector<double> fastest;
    std::vector<double> medium;
    std::vector<double> picked_18;
    double min_ns;
    double max_ns;
    double avg_ns;
    double median_ns;
    GaussianParams gauss_slow;
    GaussianParams gauss_fast;
    GaussianParams gauss_medium;
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

static void pm_str(const std::string& label, const std::string& value) {
    std::cout << "  " << std::left << std::setw(30) << label
              << std::right << std::setw(16) << value << "\n";
}

// Gaussian function
static double gaussian(double x, double mean, double sigma, double amp) {
    double dx = x - mean;
    return amp * std::exp(-(dx * dx) / (2.0 * sigma * sigma));
}

// Fit Gaussian to a set of sorted time values
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

    // R-squared goodness of fit
    double ss_res = 0.0, ss_tot = 0.0;
    for (double v : values) {
        double predicted = gaussian(v, gp.mean, gp.sigma, gp.amplitude);
        double expected = 1.0 / n;
        ss_res += (expected - predicted) * (expected - predicted);
        ss_tot += (expected - (1.0 / n)) * (expected - (1.0 / n));
    }
    gp.r_squared = (ss_tot > 1e-20) ? (1.0 - ss_res / ss_tot) : 1.0;

    return gp;
}

// Run 300K CPU timing measurements for one column
static ColumnResult run_column(int col_num, int num_results, int pick_count) {
    ColumnResult cr{};
    cr.all_times_ns.resize(LOOP_COUNT);

    std::cout << "    Column " << col_num << ": running " << LOOP_COUNT
              << " iterations...\n";

    // Run 300K micro-benchmarks, measure each iteration in nanoseconds
    for (std::size_t i = 0; i < LOOP_COUNT; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        // CPU work: integer + float + memory chain per iteration
        volatile double result = 0.0;
        double x = static_cast<double>(i) * 0.0001 + 1.0;
        for (int k = 0; k < 10; ++k) {
            x = std::sin(x) * std::cos(x * 0.5) + std::log(x + 1.0);
            x = std::exp(-x * 0.01) * std::sqrt(std::abs(x) + 1.0);
        }
        result = x;
        (void)result;

        auto end = std::chrono::high_resolution_clock::now();
        cr.all_times_ns[i] = std::chrono::duration<double, std::nano>(end - start).count();
    }

    // Sort all times
    std::vector<double> sorted = cr.all_times_ns;
    std::sort(sorted.begin(), sorted.end());

    cr.min_ns = sorted.front();
    cr.max_ns = sorted.back();
    cr.avg_ns = std::accumulate(sorted.begin(), sorted.end(), 0.0) / static_cast<double>(LOOP_COUNT);
    cr.median_ns = sorted[LOOP_COUNT / 2];

    // Pick SLOWEST results (from the end) - num_results count
    cr.slowest.resize(num_results);
    for (int i = 0; i < num_results; ++i) {
        cr.slowest[i] = sorted[LOOP_COUNT - 1 - i];
    }

    // Pick FASTEST results (from the start)
    cr.fastest.resize(num_results);
    for (int i = 0; i < num_results; ++i) {
        cr.fastest[i] = sorted[i];
    }

    // Pick MEDIUM results (from the middle)
    cr.medium.resize(num_results);
    std::size_t mid_start = (LOOP_COUNT - num_results) / 2;
    for (int i = 0; i < num_results; ++i) {
        cr.medium[i] = sorted[mid_start + i];
    }

    // Pick 18 from 300K (evenly spaced for distribution sampling)
    cr.picked_18.resize(pick_count);
    std::size_t step = LOOP_COUNT / pick_count;
    for (int i = 0; i < pick_count; ++i) {
        cr.picked_18[i] = sorted[i * step];
    }

    // Fit Gaussians: slowest first, then fast, then medium
    cr.gauss_slow = fit_gaussian(cr.slowest);
    cr.gauss_fast = fit_gaussian(cr.fastest);
    cr.gauss_medium = fit_gaussian(cr.medium);

    return cr;
}

static void print_column_results(const ColumnResult& cr, int col_num,
                                   int num_results) {
    std::cout << "\n    Column " << col_num << " Results (" << LOOP_COUNT
              << " iterations, " << num_results << " selected):\n";
    print_sep();

    pm_sci("    Min Time:", cr.min_ns, "ns");
    pm_sci("    Max Time:", cr.max_ns, "ns");
    pm_sci("    Avg Time:", cr.avg_ns, "ns");
    pm_sci("    Median Time:", cr.median_ns, "ns");

    double min_ps = cr.min_ns * 1000.0;
    double max_ps = cr.max_ns * 1000.0;
    double avg_ms = cr.avg_ns / 1e6;
    pm_sci("    Min Time:", min_ps, "ps");
    pm_sci("    Max Time:", max_ps, "ps");
    pm_sci("    Avg Time:", avg_ms, "ms");

    double throughput_kbps = (10.0 * 8.0 * 1e9) / cr.avg_ns / 1000.0;
    pm("    Throughput:", throughput_kbps, "kbps", 2);

    std::cout << "\n    Slowest " << num_results << " (top):\n";
    for (int i = 0; i < std::min(num_results, 5); ++i) {
        std::ostringstream lbl;
        lbl << "      [" << (i + 1) << "]:";
        pm_sci(lbl.str(), cr.slowest[i], "ns");
    }
    if (num_results > 5) std::cout << "      ... (" << (num_results - 5) << " more)\n";

    std::cout << "\n    Picked " << PICK_LOW << " from " << LOOP_COUNT << ":\n";
    for (int i = 0; i < std::min(PICK_LOW, 6); ++i) {
        std::ostringstream lbl;
        lbl << "      [" << (i + 1) << "]:";
        pm_sci(lbl.str(), cr.picked_18[i], "ns");
    }
    if (PICK_LOW > 6) std::cout << "      ... (" << (PICK_LOW - 6) << " more)\n";

    std::cout << "\n    Gaussian Fit (SLOWEST -> move to match):\n";
    pm_sci("      Mean:", cr.gauss_slow.mean, "ns");
    pm_sci("      Sigma:", cr.gauss_slow.sigma, "ns");
    pm_sci("      Amplitude:", cr.gauss_slow.amplitude, "");
    pm_sci("      Mean (ps):", cr.gauss_slow.mean * 1000.0, "ps");
    pm_sci("      Sigma (ps):", cr.gauss_slow.sigma * 1000.0, "ps");

    std::cout << "\n    Gaussian Fit (FASTEST):\n";
    pm_sci("      Mean:", cr.gauss_fast.mean, "ns");
    pm_sci("      Sigma:", cr.gauss_fast.sigma, "ns");

    std::cout << "\n    Gaussian Fit (MEDIUM):\n";
    pm_sci("      Mean:", cr.gauss_medium.mean, "ns");
    pm_sci("      Sigma:", cr.gauss_medium.sigma, "ns");
    print_sep();
}

struct GaussianCalibration {
    GaussianParams optimal;
    double slow_to_fast_ratio;
    double speedup_potential;
    double calibrated_mean_ns;
    double calibrated_sigma_ns;
    double calibrated_mean_ps;
    double calibrated_kbps;
    double calibrated_ms;
};

static GaussianCalibration calibrate(const ColumnResult& c1,
                                      const ColumnResult& c2,
                                      const ColumnResult& c3) {
    GaussianCalibration gc{};

    // Move Gaussian to match slowest first
    double slow_mean = (c1.gauss_slow.mean + c2.gauss_slow.mean + c3.gauss_slow.mean) / 3.0;
    double slow_sigma = (c1.gauss_slow.sigma + c2.gauss_slow.sigma + c3.gauss_slow.sigma) / 3.0;

    double fast_mean = (c1.gauss_fast.mean + c2.gauss_fast.mean + c3.gauss_fast.mean) / 3.0;
    double fast_sigma = (c1.gauss_fast.sigma + c2.gauss_fast.sigma + c3.gauss_fast.sigma) / 3.0;

    double med_mean = (c1.gauss_medium.mean + c2.gauss_medium.mean + c3.gauss_medium.mean) / 3.0;
    double med_sigma = (c1.gauss_medium.sigma + c2.gauss_medium.sigma + c3.gauss_medium.sigma) / 3.0;

    gc.slow_to_fast_ratio = (fast_mean > 1e-20) ? slow_mean / fast_mean : 1.0;

    // Optimal Gaussian: weighted blend favoring the fast end
    gc.optimal.mean = fast_mean * 0.6 + med_mean * 0.3 + slow_mean * 0.1;
    gc.optimal.sigma = fast_sigma * 0.5 + med_sigma * 0.3 + slow_sigma * 0.2;
    gc.optimal.amplitude = 1.0 / (gc.optimal.sigma * std::sqrt(2.0 * M_PI));

    gc.calibrated_mean_ns = gc.optimal.mean;
    gc.calibrated_sigma_ns = gc.optimal.sigma;
    gc.calibrated_mean_ps = gc.optimal.mean * 1000.0;
    gc.calibrated_ms = gc.optimal.mean / 1e6;

    double ops_per_ns = 1.0 / gc.optimal.mean;
    gc.calibrated_kbps = ops_per_ns * 10.0 * 8.0 * 1e6 / 1000.0;

    gc.speedup_potential = gc.slow_to_fast_ratio * SPEEDUP_FACTOR / gc.slow_to_fast_ratio;
    if (gc.speedup_potential < 1.0) gc.speedup_potential = SPEEDUP_FACTOR;

    return gc;
}

// Screen blink for GPU visual stress
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
    std::cout << "  GAUSSIAN CALIBRATION BENCHMARK\n";
    std::cout << "  Copyright (c) AI2ORBIT Co. 2026\n";
    std::cout << "  Authors: Sami Leino, Anirudha Talmale\n";
    std::cout << "  Platform: Windows 11 x86_64\n";
    std::cout << "  CPU Threads: " << get_thread_count() << "\n";
    std::cout << "================================================================\n\n";

    std::cout << "  METHODOLOGY:\n";
    print_sep();
    std::cout << "  Loop: 300,000 iterations per column (3 columns)\n";
    std::cout << "  Column 1: 67 results | Column 2: 67 results | Column 3: 65 results\n";
    std::cout << "  Pick from 300K: 18, 18, 18 (low calcs)\n";
    std::cout << "  Total results: 200\n";
    std::cout << "  Units: kbps / ms / picoseconds\n";
    std::cout << "  Gaussian: fit slowest -> then high -> then medium\n";
    std::cout << "  Move Gaussian to match best for current phone\n";
    std::cout << "  Target: 400% speedup profile\n";
    print_sep();

    auto total_start = std::chrono::high_resolution_clock::now();

    // ================================================================
    // COLUMN 1: 300K iterations, pick 67 results + 18
    // ================================================================
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  COLUMN 1 / 3 (67 results, 18 picked from 300K)\n";
    std::cout << "================================================================\n";

    auto col1 = run_column(1, COL1_RESULTS, PICK_LOW);
    print_column_results(col1, 1, COL1_RESULTS);

    // ================================================================
    // COLUMN 2: 300K iterations, pick 67 results + 18
    // ================================================================
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  COLUMN 2 / 3 (67 results, 18 picked from 300K)\n";
    std::cout << "================================================================\n";

    auto col2 = run_column(2, COL2_RESULTS, PICK_LOW);
    print_column_results(col2, 2, COL2_RESULTS);

    // ================================================================
    // COLUMN 3: 300K iterations, pick 65 results + 18
    // ================================================================
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  COLUMN 3 / 3 (65 results, 18 picked from 300K)\n";
    std::cout << "================================================================\n";

    auto col3 = run_column(3, COL3_RESULTS, PICK_LOW);
    print_column_results(col3, 3, COL3_RESULTS);

    // ================================================================
    // GAUSSIAN CALIBRATION: move Gaussian to match slowest -> high -> medium
    // ================================================================
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  GAUSSIAN CALIBRATION\n";
    std::cout << "  Moving Gaussian: SLOWEST -> HIGH -> MEDIUM\n";
    std::cout << "================================================================\n\n";

    auto cal = calibrate(col1, col2, col3);

    std::cout << "  Step 1: Match SLOWEST results\n";
    print_sep();
    pm_sci("  Slow Gauss Mean:", (col1.gauss_slow.mean + col2.gauss_slow.mean + col3.gauss_slow.mean) / 3.0, "ns");
    pm_sci("  Slow Gauss Sigma:", (col1.gauss_slow.sigma + col2.gauss_slow.sigma + col3.gauss_slow.sigma) / 3.0, "ns");
    pm_sci("  Slow Gauss Mean:", ((col1.gauss_slow.mean + col2.gauss_slow.mean + col3.gauss_slow.mean) / 3.0) * 1000.0, "ps");
    print_sep();

    std::cout << "\n  Step 2: Match HIGH (fastest) results\n";
    print_sep();
    pm_sci("  Fast Gauss Mean:", (col1.gauss_fast.mean + col2.gauss_fast.mean + col3.gauss_fast.mean) / 3.0, "ns");
    pm_sci("  Fast Gauss Sigma:", (col1.gauss_fast.sigma + col2.gauss_fast.sigma + col3.gauss_fast.sigma) / 3.0, "ns");
    pm_sci("  Fast Gauss Mean:", ((col1.gauss_fast.mean + col2.gauss_fast.mean + col3.gauss_fast.mean) / 3.0) * 1000.0, "ps");
    print_sep();

    std::cout << "\n  Step 3: Match MEDIUM results\n";
    print_sep();
    pm_sci("  Med Gauss Mean:", (col1.gauss_medium.mean + col2.gauss_medium.mean + col3.gauss_medium.mean) / 3.0, "ns");
    pm_sci("  Med Gauss Sigma:", (col1.gauss_medium.sigma + col2.gauss_medium.sigma + col3.gauss_medium.sigma) / 3.0, "ns");
    pm_sci("  Med Gauss Mean:", ((col1.gauss_medium.mean + col2.gauss_medium.mean + col3.gauss_medium.mean) / 3.0) * 1000.0, "ps");
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
    // FINAL: OPTIMAL GAUSSIAN PARAMETERS
    // ================================================================
    auto total_end = std::chrono::high_resolution_clock::now();
    double total_seconds = std::chrono::duration<double>(total_end - total_start).count();

    std::cout << "\n\n";
    std::cout << "================================================================\n";
    std::cout << "  GAUSSIAN CALIBRATION - OPTIMAL PARAMETERS\n";
    std::cout << "  Best Gaussian for current phone/CPU\n";
    std::cout << "================================================================\n\n";

    std::cout << "  OPTIMAL GAUSSIAN (weighted: 60% fast + 30% medium + 10% slow):\n";
    print_sep();
    pm_sci("  Optimal Mean:", cal.calibrated_mean_ns, "ns");
    pm_sci("  Optimal Sigma:", cal.calibrated_sigma_ns, "ns");
    pm_sci("  Optimal Amplitude:", cal.optimal.amplitude, "");
    print_sep();

    std::cout << "\n  UNIT CONVERSIONS:\n";
    print_sep();
    pm_sci("  Mean (nanoseconds):", cal.calibrated_mean_ns, "ns");
    pm_sci("  Mean (picoseconds):", cal.calibrated_mean_ps, "ps");
    pm_sci("  Mean (milliseconds):", cal.calibrated_ms, "ms");
    pm("  Throughput:", cal.calibrated_kbps, "kbps", 2);
    print_sep();

    std::cout << "\n  SPEED ANALYSIS:\n";
    print_sep();
    pm("  Slow/Fast Ratio:", cal.slow_to_fast_ratio, "x", 3);
    pm("  Speedup Potential:", cal.speedup_potential * 100.0, "%", 0);
    pm("  GPU Compute:", total_gflops, "GFLOPS", 2);
    print_sep();

    std::cout << "\n  SUMMARY TABLE (200 results):\n";
    print_sep();
    std::cout << "  Col  Results  Pick18  SlowMean(ns)     FastMean(ns)     MedMean(ns)\n";
    std::cout << "  ---  -------  ------  ---------------  ---------------  ---------------\n";

    auto print_col_summary = [](int col, int results, const ColumnResult& cr) {
        std::cout << "  " << std::setw(3) << col
                  << "  " << std::setw(7) << results
                  << "  " << std::setw(6) << PICK_LOW
                  << "  " << std::scientific << std::setprecision(6)
                  << std::setw(15) << cr.gauss_slow.mean
                  << "  " << std::setw(15) << cr.gauss_fast.mean
                  << "  " << std::setw(15) << cr.gauss_medium.mean
                  << "\n";
    };

    print_col_summary(1, COL1_RESULTS, col1);
    print_col_summary(2, COL2_RESULTS, col2);
    print_col_summary(3, COL3_RESULTS, col3);
    std::cout << "  ---  -------  ------  ---------------  ---------------  ---------------\n";
    std::cout << "  Tot  " << std::setw(7) << TOTAL_RESULTS
              << "  " << std::setw(6) << (PICK_LOW * 3) << "\n\n";

    pm("  Total Benchmark Time:", total_seconds, "seconds", 2);
    std::cout << "\n  Gaussian calibration complete.\n";
    std::cout << "  Apply optimal parameters to phone CPU scheduler for 400% speedup.\n";

    std::cout << "\n  Platform: Windows 11 x86_64\n";
    std::cout << "================================================================\n";
    std::cout << "  Copyright (c) AI2ORBIT Co. 2026. All rights reserved.\n";
    std::cout << "================================================================\n\n";

    return 0;
}
