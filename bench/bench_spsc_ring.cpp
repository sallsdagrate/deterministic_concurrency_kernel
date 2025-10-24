#include <iostream>
#include <chrono>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>
#include "../core/spsc_ring.hpp"

struct Timestamp {
    std::chrono::steady_clock::time_point t;
};

template<class T>
void test_throughput(SpscRing<T>& q, const T& val, T& out, const size_t n_ops){
    auto start = std::chrono::steady_clock::now();
    for(size_t i = 0; i < n_ops; i++) {
        while (!q.try_push(val)) {}
        while (!q.try_pop(out)) {}
    }
    auto end = std::chrono::steady_clock::now();
    double time_taken = std::chrono::duration<double>(end - start).count();
    
    std::cout << "n operations: " << n_ops << " time: " << time_taken << "s" << " ops/s: " << (n_ops/time_taken) << std::endl;
}

template<class T>
void test_minimal_concurrent_throughput(SpscRing<T>& q, const T& val, const size_t n_ops) {
    std::atomic<bool> stop_flag{false};
    size_t pop_count = 0;

    std::thread consumer([&](){
        T local_out;
        size_t local_pop_count = 0;

        while(!stop_flag.load(std::memory_order_relaxed)) {
            if (q.try_pop(local_out)) local_pop_count += 1;
        }

        while (q.try_pop(local_out)) local_pop_count += 1;

        pop_count = local_pop_count;
    });

    auto start = std::chrono::steady_clock::now();
    
    for(size_t i = 0; i < n_ops; i++) {
        while (!q.try_push(val)) {}
    }
    stop_flag.store(true, std::memory_order_relaxed);
    consumer.join();

    auto end = std::chrono::steady_clock::now();
    double time_taken = std::chrono::duration<double>(end - start).count();
    
    std::cout << "n pushes: " << n_ops << " | n pops: " << pop_count << " | time: " << time_taken << "s" << " | ops/s: " << (pop_count/time_taken) << std::endl;

}

void test_minimal_concurrent_latency(SpscRing<Timestamp>& q, const size_t n_ops) {
    using clock = std::chrono::steady_clock;
    std::atomic<bool> stop_flag{false};
    std::vector<uint64_t> latencies;
    latencies.reserve(n_ops);

    std::thread consumer([&](){
        Timestamp local_out;

        while(!stop_flag.load(std::memory_order_relaxed)) {
            if (q.try_pop(local_out)) {
                auto lat_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - local_out.t);
                latencies.emplace_back(lat_ns.count());
            };
        }

        while (q.try_pop(local_out)) {
            auto lat_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - local_out.t);
            latencies.emplace_back(lat_ns.count());
        }

    });

    auto start = std::chrono::steady_clock::now();
    
    for(size_t i = 0; i < n_ops; i++) {
        Timestamp ts {clock::now()};
        while (!q.try_push(ts)) {}
    }
    stop_flag.store(true, std::memory_order_relaxed);
    consumer.join();

    if (latencies.empty()) {
        std::cout << "no samples\n";
        return;
    }

    std::sort(latencies.begin(), latencies.end());
    auto percentile = [&](double p) {
        return latencies[static_cast<size_t>((p/100.0) * (latencies.size() - 1))];
    };
    std::cout << "latencies (ns) - min:" << latencies.front() 
    << " | p50: " << percentile(50) 
    << " | p95: " << percentile(95) 
    << " | p99: " << percentile(99) 
    << " | max: " << latencies.back() 
    << std::endl;
}

int main() {
    SpscRing<Timestamp> q (1<<10);
    // int x = 100, out;
    const size_t n_ops = 1<<25;
    test_minimal_concurrent_latency(q, n_ops);
}

