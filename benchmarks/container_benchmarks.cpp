#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>

using Clock = std::chrono::steady_clock;

void benchmark_map_insert(std::size_t n) {
    std::vector<std::uint64_t> keys;
    keys.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        keys.push_back(static_cast<std::uint64_t>(i));
    }

    std::map<std::uint64_t, std::uint64_t> values;

    const auto start = Clock::now();

    for (const auto key : keys) {
        values.emplace(key, key);
    }

    const auto end = Clock::now();

    const auto elapsed_ns = 
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start
    ).count();

    const double ns_per_operation =
        static_cast<double>(elapsed_ns) / 
        static_cast<double>(n);

    std::cout
        << "std::map insert"
        << " | N = " << n
        << " | total = " << elapsed_ns << " ns"
        << " | average = " << ns_per_operation << " ns/op"
        << " | final size = " << values.size()
        << '\n';
}

int main() {
    for (const std::size_t n : {1'000, 10'000, 100'000}) {
        benchmark_map_insert(n);
    }

    return 0;
}