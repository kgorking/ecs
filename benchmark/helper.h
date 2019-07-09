#pragma once
#include <algorithm>
#include <random>


// see here: http://stackoverflow.com/questions/28287064/how-not-to-optimize-away-mechanics-of-a-folly-function
#pragma optimize("", off)
template <class T>
void escape(T&& datum) { datum = datum; }
#pragma optimize("", on)


constexpr int num_runs = 100;
constexpr int num_iterations = 100;

constexpr int set_size_1 = 250;
constexpr int set_size_2 = 250;
constexpr int set_size_3 = 250;

template<typename T>
static T random(T min, T max) {
	// Seed with a real random value, if available
	static std::random_device r;

	// Choose a random mean between min and max
	static std::default_random_engine e1(r());

	const std::uniform_int_distribution<T> uniform_dist(min, max);

	return uniform_dist(e1);
}

static auto make_vector(int size)
{
	std::vector<int> vec(size);
	std::generate(vec.begin(), vec.end(), [size]() { return random(0, size<<1); });
	std::sort(vec.begin(), vec.end());
	vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
	return vec;
}

// Test-vectors
extern std::vector<int> const vector_1;
extern std::vector<int> const vector_2;
extern std::vector<int> const vector_3;
extern std::vector<int> const vector_4;
extern std::vector<int> const vector_5;
