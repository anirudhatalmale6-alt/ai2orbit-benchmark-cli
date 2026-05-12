# AI2ORBIT BenchmarkCore - Makefile
# Builds on Linux (Gentoo, Ubuntu, Arch, etc.) and cross-compiles for Windows
#
# Usage:
#   make              - build all Linux binaries
#   make windows      - cross-compile all Windows .exe
#   make install      - install to /usr/local/bin
#   make clean        - remove build artifacts

CXX         ?= g++
MINGW_CXX   ?= x86_64-w64-mingw32-g++
CXXFLAGS    ?= -std=c++17 -O2 -Wall
LDFLAGS     ?= -lpthread
INSTALL_DIR ?= /usr/local/bin

LIB_SRC = lib/cpu_benchmark.cpp lib/memory_benchmark.cpp lib/dram_mapper.cpp lib/cuda_benchmark.cpp
LIB_INC = -Ilib

TARGETS_LINUX  = ai2orbit_benchmark ai2orbit_benchmark_v2 ai2orbit_benchmark_v3
TARGETS_WIN    = AI2ORBIT_Benchmark.exe AI2ORBIT_Benchmark_V2.exe AI2ORBIT_Benchmark_V3.exe

.PHONY: all linux windows install uninstall clean

all: linux

linux: $(TARGETS_LINUX)

ai2orbit_benchmark: benchmark_cli.cpp $(LIB_SRC)
	$(CXX) $(CXXFLAGS) $(LIB_INC) -o $@ $^ $(LDFLAGS)

ai2orbit_benchmark_v2: benchmark_cli_v2.cpp $(LIB_SRC)
	$(CXX) $(CXXFLAGS) $(LIB_INC) -o $@ $^ $(LDFLAGS)

ai2orbit_benchmark_v3: benchmark_cli_v3.cpp $(LIB_SRC)
	$(CXX) $(CXXFLAGS) $(LIB_INC) -o $@ $^ $(LDFLAGS)

windows: $(TARGETS_WIN)

AI2ORBIT_Benchmark.exe: benchmark_cli.cpp $(LIB_SRC)
	$(MINGW_CXX) $(CXXFLAGS) $(LIB_INC) -static -o $@ $^ $(LDFLAGS)

AI2ORBIT_Benchmark_V2.exe: benchmark_cli_v2.cpp $(LIB_SRC)
	$(MINGW_CXX) $(CXXFLAGS) $(LIB_INC) -static -o $@ $^ $(LDFLAGS)

AI2ORBIT_Benchmark_V3.exe: benchmark_cli_v3.cpp $(LIB_SRC)
	$(MINGW_CXX) $(CXXFLAGS) $(LIB_INC) -static -o $@ $^ $(LDFLAGS)

install: linux
	install -d $(DESTDIR)$(INSTALL_DIR)
	install -m 755 ai2orbit_benchmark $(DESTDIR)$(INSTALL_DIR)/ai2orbit-benchmark
	install -m 755 ai2orbit_benchmark_v2 $(DESTDIR)$(INSTALL_DIR)/ai2orbit-benchmark-v2
	install -m 755 ai2orbit_benchmark_v3 $(DESTDIR)$(INSTALL_DIR)/ai2orbit-benchmark-v3

uninstall:
	rm -f $(DESTDIR)$(INSTALL_DIR)/ai2orbit-benchmark
	rm -f $(DESTDIR)$(INSTALL_DIR)/ai2orbit-benchmark-v2
	rm -f $(DESTDIR)$(INSTALL_DIR)/ai2orbit-benchmark-v3

clean:
	rm -f $(TARGETS_LINUX) $(TARGETS_WIN)
