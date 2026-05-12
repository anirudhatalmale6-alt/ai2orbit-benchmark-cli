#!/bin/bash
# AI2ORBIT BenchmarkCore - Installer
# Tested on: Gentoo, Ubuntu, Debian, Arch, Fedora
# Copyright (c) AI2ORBIT Co. 2026

set -e

BOLD="\033[1m"
GREEN="\033[32m"
CYAN="\033[36m"
RED="\033[31m"
RESET="\033[0m"

PREFIX="${PREFIX:-/usr/local}"
BINDIR="${PREFIX}/bin"
LIBDIR="${PREFIX}/lib/ai2orbit-benchmark"
SRCDIR="$(cd "$(dirname "$0")" && pwd)"

echo -e "${CYAN}${BOLD}"
echo "    ___   ____  ___   ____  ____  ____  ______"
echo "   /   | /  _/ / _ \\ / __ \\/ __ )/ / / /_  __/"
echo "  / /| | / /  / /_/ // /_/ / __ / / / / / /"
echo " / ___ |/ /  /  ___// _, _/ /_/ / /_/ / / /"
echo "/_/  |_/___/ /_/   /_/ |_/_____/\\____/ /_/"
echo ""
echo "  BenchmarkCore Installer"
echo "  Copyright (c) AI2ORBIT Co. 2026"
echo -e "${RESET}"

detect_distro() {
    if [ -f /etc/gentoo-release ]; then
        echo "gentoo"
    elif [ -f /etc/debian_version ]; then
        echo "debian"
    elif [ -f /etc/arch-release ]; then
        echo "arch"
    elif [ -f /etc/fedora-release ]; then
        echo "fedora"
    elif [ -f /etc/redhat-release ]; then
        echo "redhat"
    else
        echo "unknown"
    fi
}

DISTRO=$(detect_distro)
echo -e "  ${BOLD}Detected:${RESET} ${DISTRO}"
echo -e "  ${BOLD}Prefix:${RESET}   ${PREFIX}"
echo ""

check_deps() {
    local missing=()

    if ! command -v g++ &>/dev/null; then
        missing+=("g++")
    fi

    if ! command -v make &>/dev/null; then
        missing+=("make")
    fi

    if [ ${#missing[@]} -eq 0 ]; then
        echo -e "  ${GREEN}All dependencies satisfied.${RESET}"
        return 0
    fi

    echo -e "  ${RED}Missing: ${missing[*]}${RESET}"
    echo ""

    case "$DISTRO" in
        gentoo)
            echo -e "  ${BOLD}Install with:${RESET}"
            echo "    emerge --ask sys-devel/gcc sys-devel/make"
            echo ""
            echo "  For Windows cross-compilation (optional):"
            echo "    emerge --ask cross-x86_64-w64-mingw32/gcc"
            ;;
        debian)
            echo -e "  ${BOLD}Install with:${RESET}"
            echo "    apt install build-essential"
            echo "    apt install g++-mingw-w64-x86-64  # optional, for Windows"
            ;;
        arch)
            echo -e "  ${BOLD}Install with:${RESET}"
            echo "    pacman -S base-devel"
            echo "    pacman -S mingw-w64-gcc  # optional, for Windows"
            ;;
        fedora)
            echo -e "  ${BOLD}Install with:${RESET}"
            echo "    dnf install gcc-c++ make"
            echo "    dnf install mingw64-gcc-c++  # optional, for Windows"
            ;;
        *)
            echo -e "  ${BOLD}Install g++ and make for your distribution.${RESET}"
            ;;
    esac

    echo ""
    return 1
}

echo -e "${BOLD}  Checking dependencies...${RESET}"
if ! check_deps; then
    echo -e "  ${RED}Please install missing dependencies and re-run.${RESET}"
    exit 1
fi

echo ""
echo -e "${BOLD}  Building...${RESET}"
cd "$SRCDIR"
make clean 2>/dev/null || true
make linux CXX="${CXX:-g++}"

echo ""
echo -e "${BOLD}  Installing to ${BINDIR}...${RESET}"

if [ -w "$BINDIR" ]; then
    make install PREFIX="$PREFIX"
else
    echo "  (requires root)"
    sudo make install PREFIX="$PREFIX"
fi

echo ""
echo -e "${BOLD}  Installing library sources to ${LIBDIR}...${RESET}"

if [ -w "$(dirname "$LIBDIR")" ]; then
    mkdir -p "$LIBDIR"
    cp lib/*.h lib/*.cpp "$LIBDIR/"
else
    sudo mkdir -p "$LIBDIR"
    sudo cp lib/*.h lib/*.cpp "$LIBDIR/"
fi

echo ""
echo -e "${GREEN}${BOLD}  Installation complete!${RESET}"
echo ""
echo "  Binaries:"
echo "    ai2orbit-benchmark      (V1/V4)"
echo "    ai2orbit-benchmark-v2   (Equality of the Oinkers)"
echo "    ai2orbit-benchmark-v3   (Gaussian Calibration)"
echo ""
echo "  Library sources:"
echo "    ${LIBDIR}/"
echo ""
echo "  Run:"
echo "    ai2orbit-benchmark-v3"
echo ""

if [ "$DISTRO" = "gentoo" ]; then
    echo -e "  ${CYAN}Gentoo tip: To add to your overlay, use the ebuild at:${RESET}"
    echo "    ${SRCDIR}/ai2orbit-benchmark.ebuild"
    echo ""
fi
