#ifndef MINHEAPTIMER_H
#define MINHEAPTIMER_H

#include <queue>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <chrono>
#include <vector>

using TimeoutCallBack = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using TimeStamp = Clock::time_point;

struct TimerNode
{
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
};

// 小根堆比较器：expires 小的优先
struct TimerCmp 
{
    bool operator()(const TimerNode& a, const TimerNode& b) const 
    {
        return a.expires > b.expires;  // 小根堆
    }
};



class MinHeapTimer
{

public:
    MinHeapTimer() = default;
    ~MinHeapTimer() { clear(); }

    void add(int id, int timeout, const TimeoutCallBack& cb);
    void adjust(int id, int timeout);
    void del(int id);
    void clear();
    void tick();
    void pop();
    int getNextTick();

private:
    std::priority_queue<TimerNode, std::vector<TimerNode>, TimerCmp> heap;
    std::unordered_map<int, TimerNode> nodes;
};

#endif