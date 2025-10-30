#pragma once

#include <atomic>
#include <new>

template <class T>
class SpscRing {
    static constexpr size_t cacheLineSize = 64;
    
    // test performance with and without aligning
    alignas(cacheLineSize) std::atomic<size_t> m_head; // alignas(cacheLineSize)
    alignas(cacheLineSize) std::atomic<size_t> m_tail; // alignas(cacheLineSize)
    const size_t m_capacity, m_mask;
    T* m_buffer;

    public:
    
    explicit SpscRing(size_t capacity): 
    m_capacity(capacity), 
    m_mask(capacity - 1), 
    m_buffer(static_cast<T*>(::operator new[](capacity * sizeof(T)))) 
    {}

    ~SpscRing() {
        for (size_t i = m_tail.load(); i != m_head.load(); i = (i+1) & m_mask) m_buffer[i].~T();
        ::operator delete[](m_buffer);
    }

    bool try_push(const T& x) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t next = (head+1) & m_mask;
        if (next == m_tail.load(std::memory_order_acquire)) return false;
        new (&m_buffer[head]) T(x);
        m_head.store(next, std::memory_order_release);
        return true;
    }

    bool try_pop(T& out) {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) return false;
        out = std::move(m_buffer[tail]);
        m_buffer[tail].~T();
        m_tail.store((tail+1) & m_mask, std::memory_order_release);
        return true;
    }

    bool empty() {
        return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
    }
};