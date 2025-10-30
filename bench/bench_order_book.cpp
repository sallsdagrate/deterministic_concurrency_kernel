#include "../core/order_book.hpp"
#include "../core/spsc_ring.hpp"

#include <chrono>
#include <vector>
#include <iostream>
#include <random>
#include <thread>
#include <atomic>
#include <algorithm>


void log_trades (const std::vector<Trade> trades) {
    std::cout << "Trades\n";
    for (auto trade : trades) {
        std::cout << trade.quantity <<  "@" << trade.price << "(" << trade.seller_id << "->" << trade.buyer_id << ") | ";
    }
    std::cout << "\n\n";
} 

void bench_order_book (SpscRing<Event>& buffer, OrderBook& book, std::vector<Trade>& trades_out, BookStats& stats, size_t n_events = 1024, long unsigned int seed = 0) {
    // shared data
    trades_out.reserve(1024);
    stats.latencies_ns.reserve(1024);

    std::atomic_bool finished_producing {false};

    std::thread consumer([&](){
        while (!finished_producing.load(std::memory_order_acquire) || !buffer.empty()) {
            // try pop and process events
            Event e; 
            if (buffer.try_pop(e)){

                auto timestamp_out = std::chrono::steady_clock::now();
                stats.latencies_ns.emplace_back(std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp_out - e.timestamp_in).count());

                stats.consumed++;

                // handle event with order book
                switch (e.type)
                {
                case Type::New:
                    book.on_new(e, trades_out);
                    stats.consumed_new++;
                    break;
                case Type::Cancel:
                    // std::cout << "cancelling " << e.order_id << " | "; 
                    book -= (e.order_id);
                    stats.consumed_cancel++;
                    break;
                case Type::Replace:
                    // std::cout << "replacing " << e.order_id << " | "; 
                    book.on_replace(e, trades_out);
                    stats.consumed_replace++;
                    break;
                default:
                    break;
                }
            }
            else {
                std::this_thread::yield();
            }
        }
    });

    std::thread producer([&](){
        int curr_oid = 0;
        
        constexpr float new_bar = 0.8f, cancel_bar = 1.0f;
        std::mt19937_64 rd{seed};
        std::uniform_real_distribution<float> type_sampler(0.0, 1.0);
        std::normal_distribution price_sampler{100.f, 5.f};
        std::uniform_int_distribution qty_sampler(1, 100);
        std::uniform_int_distribution side_sampler(1, 2);
        

        for (int i = 0; i < n_events; i++) {

            // randomly sample event
            Event e;
            
            float type_rv = (i == 0) ? 0.0f : type_sampler(rd);
            if (type_rv <= new_bar) {
                e.type = Type::New;
                stats.produced_new++;
            }
            else if (type_rv <= cancel_bar) {
                e.type = Type::Cancel;
                stats.produced_cancel++;
            }
            else {
                e.type = Type::Replace;
                stats.produced_replace++;
            }
            
            e.seq = i;
            if (e.type == Type::Cancel) {
                std::uniform_int_distribution id_sampler(0, curr_oid);
                e.order_id = id_sampler(rd);
            }
            else {
                e.order_id = curr_oid++;
            }
            e.side = side_sampler(rd) == 1 ? Side::Buy : Side::Sell;
            e.price = std::round(price_sampler(rd));
            e.quantity = qty_sampler(rd);
            e.timestamp_in = std::chrono::steady_clock::now();

            // add to buffer
            while (!buffer.try_push(e)) {};
            stats.produced++;
        }
        finished_producing.store(true, std::memory_order_release);
    });

    producer.join();
    consumer.join();

    
}

int main() {
    SpscRing<Event> buffer(1024);
    OrderBook book;
    BookStats stats;
    std::vector<Trade> trades_out;
    int n_events = 1<<22;

    std::chrono::time_point start = std::chrono::steady_clock::now();

    bench_order_book(buffer, book, trades_out, stats, n_events, 0);

    std::chrono::time_point end = std::chrono::steady_clock::now();

    auto time_taken_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    size_t n_trades = trades_out.size();
    
    auto percentile = [](const std::vector<uint64_t>& lats, const double p) {
        return lats[static_cast<size_t>((p/100.0) * (lats.size() - 1))];
    };

    // book.log_books();
    // log_trades(trades_out);

    std::sort(stats.latencies_ns.begin(), stats.latencies_ns.end());
    std::cout << "latencies (ns)\nmin:" << stats.latencies_ns.front() 
    << " | p50: " << percentile(stats.latencies_ns, 50) 
    << " | p95: " << percentile(stats.latencies_ns, 95) 
    << " | p99: " << percentile(stats.latencies_ns, 99) 
    << " | max: " << stats.latencies_ns.back()
    << std::endl;


    std::cout << "throughput\n"
    << stats.produced << " events, " << time_taken_ms << " ms, " << n_events/time_taken_ms << " events/ms\n"
    << n_trades << " trades, " << time_taken_ms << " ms, " << n_trades/time_taken_ms << " trades/ms\n";

    return 0;
}