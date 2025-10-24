#include <vector>
#include <cstdint>
#include <deque>

struct TWNode {
    uint64_t timestamp;
    uint64_t label;
    uint8_t active;
    uint32_t next_node_in_bucket;
};

struct TWHandle {
    uint32_t idx;
};

class TimerWheel {
    std::deque<TWNode> nodes;
    std::vector<std::vector<std::vector<TWHandle>>> tiers;
    int m_resolution, m_wheelsize;

    public:

    TimerWheel (int resolution, int wheel_size): m_resolution(resolution), m_wheelsize(wheel_size) {
        tiers.reserve(4);
        for (int i = 0; i < 4; i++) tiers[i].reserve(wheel_size);
    }

    TWHandle add (uint64_t timestamp, uint64_t label) {
        uint32_t idx = (uint32_t) nodes.size();
        nodes.push_back({timestamp, label, 1, UINT32_MAX});
        // push to bucket
        return {idx};
    }

    bool cancel (TWHandle handle) {
        if (handle.idx > nodes.size()) return false;
        auto &n = nodes[handle.idx];
        if (!n.active)
        n.active = 0;
        return true;
    }




}