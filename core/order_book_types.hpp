#pragma once
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <atomic>

using OrderId = uint64_t;
using Price = uint32_t;
using Timestamp = std::chrono::steady_clock::time_point;

enum class Side {
    Buy,
    Sell
};

enum class Type {
    New,
    Cancel,
    Replace
};

// event coming from the exchange
struct Event {
    uint64_t seq;
    Type type;
    OrderId order_id;
    Side side;
    Price price;
    int32_t quantity;
    Timestamp timestamp_in;
};

// currently active order sitting in the book
struct Order {
    OrderId order_id;
    Side side;
    Price price;
    int32_t quantity_remaining;
    uint64_t seq_new;
    bool active;

    Order (OrderId o, Side s, Price p, int32_t q, uint64_t se, bool a):
    order_id{o}, side{s}, price{p}, quantity_remaining{q}, seq_new{se}, active{a}
    {}
};

// fill between two orders
struct Trade {
    OrderId seller_id;
    OrderId buyer_id;
    Price price;
    int32_t quantity;
    Timestamp timestamp_exec;

    Trade (OrderId s, OrderId (b), Price p, int32_t q, Timestamp ts): 
    seller_id{s}, buyer_id{b}, price{p}, quantity{q}, timestamp_exec{ts} 
    {}
};

// used for quick lookup
struct OrderMeta {
    Price price;
    size_t fifo_idx;
    bool active;
};

struct BookStats {
    size_t produced = 0;
    size_t produced_new = 0;
    size_t produced_cancel = 0;
    size_t produced_replace = 0;

    size_t consumed = 0;
    size_t consumed_new = 0;
    size_t consumed_cancel = 0;
    size_t consumed_replace = 0;

    std::vector<uint64_t> latencies_ns;
};
