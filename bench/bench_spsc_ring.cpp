#include <iostream>
#include <chrono>
#include <atomic>
#include <thread>
#include "../core/spsc_ring.hpp"

struct timestamp {

}

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
void test_minimal_concurrent_throughput_with_timestamps(SpscRing<T>& q, const T& val, std::vector<T>& message_buffer, const size_t n_ops) {
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

int main() {
    SpscRing<int> q (1<<12);
    int x = 100, out;
    const size_t n_ops = 1<<25;
    test_minimal_concurrent_throughput_with_timestamps<int>(q, x, n_ops);
}

