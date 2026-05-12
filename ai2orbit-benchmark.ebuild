# Copyright 2026 AI2ORBIT Co.
# Distributed under the terms of the GNU General Public License v2

EAPI=8

DESCRIPTION="AI2ORBIT BenchmarkCore - CPU/DRAM/GPU calibration with Gaussian analysis"
HOMEPAGE="https://ai2orbit.com"

LICENSE="all-rights-reserved"
SLOT="0"
KEYWORDS="~amd64 ~x86"

DEPEND="
	sys-devel/gcc
	sys-devel/make
"
RDEPEND=""

src_compile() {
	emake linux CXX="$(tc-getCXX)"
}

src_install() {
	dobin ai2orbit_benchmark
	dobin ai2orbit_benchmark_v2
	dobin ai2orbit_benchmark_v3

	insinto /usr/lib/ai2orbit-benchmark
	doins lib/*.h lib/*.cpp
}
