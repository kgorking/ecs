#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <immintrin.h>
#include <numeric>
#include <random>

static int find_first_set(unsigned x) noexcept {
	// return 1 + std::countr_zero((unsigned)(x & (-x)));
	return std::popcount(x ^ ~-x);
}

// k[j] >>= find_first_set(~k[j]);
template <size_t N>
void adjust_k(std::array<size_t, N>& k) {
	for (int i = 0; i < N; i++)
		k[i] >>= std::popcount(~k[i] ^ ~-~k[i]);
}

// play with the array size (n) and compiler versions
const unsigned n = 1e7, m = 2e5;

int a[n], q[m], results[m];
volatile unsigned iters = std::log2(n + 1);
alignas(64) int t[n + 1];

void build(int k = 1) {
	static int i = 0;
	if (k <= n) {
		build(2 * k);
		t[k] = a[i++];
		build(2 * k + 1);
	}
}

int baseline(int x) {
	return *std::lower_bound(a, a + n, x);
}

int branchless(int x) {
	int *base = a, len = n;
	while (len > 1) {
		int half = len / 2;
		_mm_prefetch((const char*)&base[len / 2], _MM_HINT_T0);
		_mm_prefetch((const char*)&base[half + len / 2], _MM_HINT_T0);
		base = (base[half] < x ? &base[half] : base);
		len -= half;
	}
	return *(base + (*base < x));
}

int branchless2(int x) {
	int *base = a, len = n;
	while (len > 1) {
		int half = len / 2;
		base = base + half * (base[half - 1] < x);
		len = len - half;
	}
	return *base;
}

int eytzinger(int x) {
	uintptr_t k = 1;
	while (k <= n) {
		//_mm_prefetch((const char *)t + k * 16, _MM_HINT_T0);
		k = 2 * k + (unsigned(t[k] - x) >> 31);
	}
	k >>= find_first_set(~k);
	return t[k];
}

int eytzinger2(int x) {
	unsigned k = 1;

	for (int i = 0; i < iters; i++) {
		_mm_prefetch((const char*)t + k * 16, _MM_HINT_T0);
		k = 2 * k + (t[k] < x);
	}

	k = 2 * k + (t[k * (k <= n)] < x);
	k >>= find_first_set(~k);

	return t[k];
}

int eytzinger3(int x) {
	unsigned k = 1;

	for (int i = 0; i < iters - 3; i++) {
		_mm_prefetch((const char*)t + k * 16, _MM_HINT_T0);
		k = 2 * k + (t[k] < x);
	}

	k = 2 * k + (t[k] < x);
	k = 2 * k + (t[k] < x);
	k = 2 * k + (t[k] < x);
	k = 2 * k + (t[k * (k <= n)] < x);
	k >>= find_first_set(~k);

	return t[k];
}

int eytzinger4(int x) {
	size_t k = 1;

	size_t i = 0;
	for (; i < iters - 4; i += 4) {
		_mm_prefetch((const char*)t + k * 16, _MM_HINT_T0);
		k = 2 * k + (t[k] < x);
		k = 2 * k + (t[k] < x);
		k = 2 * k + (t[k] < x);
		k = 2 * k + (t[k] < x);
	}

	for (; i < iters; i += 1) {
		_mm_prefetch((const char*)t + k * 16, _MM_HINT_T0);
		k = 2 * k + (t[k] < x);
	}

	k = 2 * k + (t[k * (k <= n)] < x);
	k >>= find_first_set(~k);

	return t[k];
}

template <size_t N>
static int eytzinger_x(std::array<int, N> const& x) {
	std::array<unsigned, N> k;
	int ret = 0;

	for (size_t j = 0; j < N; j++)
		k[j] = 2 + (t[1] < x[j]);

	for (size_t i = 1; i < iters; i++) {
		for (size_t j = 0; j < N; j++) {
			k[j] = (2 * k[j]) + ((t[k[j]] < x[j]));
		}
	}

	for (size_t j = 0; j < N; j++) {
		int const off_t = t[(k[j] <= n) * k[j]];
		k[j] = 2 * k[j] + (off_t < x[j]);
		k[j] >>= std::popcount(k[j] ^ -~k[j]);
		ret += t[k[j]];
	}

	return (int)ret;
}

float timeit(int (*lower_bound)(int)) {
	clock_t start = clock();

	int checksum = 0;

	for (int i = 0; i < m; i++)
		checksum += lower_bound(q[i]);

	float duration = float(clock() - start) / CLOCKS_PER_SEC;

	printf("  checksum: %d\n", checksum);
	printf("  latency: %.2fns\n", 1e9 * duration / m);

	return duration;
}

template <size_t N = 16>
float timeit_x(int (*lower_bound)(std::array<int, N> const&)) {
	clock_t start = clock();

	int checksum = 0;

	for (int i = 0; i < m; i += N)
		checksum += lower_bound(*(std::array<int, N>*)(q + i));

	float duration = float(clock() - start) / CLOCKS_PER_SEC;

	printf("  checksum: %d\n", checksum);
	printf("  latency: %.2fns\n", 1e9 * duration / m);

	return duration;
}

int main() {
	printf("iterations: %d\n", iters);

	for (int i = 0; i < n; i++)
		a[i] = rand(); // <- careful on 16-bit platforms
	for (int i = 0; i < m; i++)
		q[i] = rand();

	a[0] = RAND_MAX; // to avoid dealing with end-of-array iterators
	t[0] = -1;		 // an element that is less than x

	std::sort(a, a + n);
	build();

	printf("std::lower_bound:\n");
	float x = timeit(baseline);

	printf("branchless:\n");
	printf("  speedup: %.2fx\n", x / timeit(branchless));

	printf("branchless v2:\n");
	printf("  speedup: %.2fx\n", x / timeit(branchless2));

	printf("eytzinger:\n");
	printf("  speedup: %.2fx\n", x / timeit(eytzinger));

	printf("eytzinger v2:\n");
	printf("  speedup: %.2fx\n", x / timeit(eytzinger2));

	printf("eytzinger v3:\n");
	printf("  speedup: %.2fx\n", x / timeit(eytzinger3));

	printf("eytzinger v4:\n");
	printf("  speedup: %.2fx\n", x / timeit(eytzinger4));

	printf("eytzinger x4:\n");
	printf("  speedup: %.2fx\n", x / timeit_x(eytzinger_x));

	//printf("eytzinger avx512:\n");
	//printf("  speedup: %.2fx\n", x / timeit_x(eytzinger_avx512));

	std::sort(q, q + m);
	printf("eytzinger x4 sort q:\n");
	printf("  speedup: %.2fx\n", x / timeit_x(eytzinger_x));

	return 0;
}
