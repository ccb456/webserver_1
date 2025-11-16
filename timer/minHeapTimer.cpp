#include "minHeapTimer.h"

void MinHeapTimer::add(int id, int timeout, const TimeoutCallBack& cb)
{
    TimeStamp newTime = Clock::now() + MS(timeout);

    TimerNode node{id, newTime, cb};

    nodes[id] = node;
    heap.push(node);
}

void MinHeapTimer::adjust(int id, int timeout)
{
    if(nodes.count(id) == 0) return;

    TimeStamp newTime = Clock::now() + MS(timeout);
    nodes[id].expires = newTime;
    heap.push(nodes[id]);
}

void MinHeapTimer::del(int id)
{
    if(nodes.count(id) == 0) return;

    nodes[id].cb();
    nodes.erase(id);
}

void MinHeapTimer::pop()
{
    while(!heap.empty())
    {
        TimerNode top = heap.top();

        // 是否是最新版本
        if(nodes.count(top.id) && nodes[top.id].expires == top.expires)
        {
            nodes.erase(top.id);
            heap.pop();
            return;
        }

        // 旧版本
        heap.pop();
    }
}

void MinHeapTimer::tick()
{
    TimeStamp now = Clock::now();

    while(!heap.empty())
    {
        TimerNode top = heap.top();

        // 旧版本的定时器
        if(!nodes.count(top.id) || nodes[top.id].expires != top.expires)
        {
            heap.pop();
            continue;
        }

        // 未到期
        if(std::chrono::duration_cast<MS>(top.expires - now).count() > 0)
        {
            break;
        }

        // 执行回调
        top.cb();
        heap.pop();
        nodes.erase(top.id);
    }

}

int MinHeapTimer::getNextTick()
{
    tick();

    if(heap.empty()) return -1;

    while(!heap.empty())
    {
        TimerNode top = heap.top();
        if (!nodes.count(top.id) || nodes[top.id].expires != top.expires) 
        {
            heap.pop();
            continue;
        }

        int res = std::chrono::duration_cast<MS>(top.expires - Clock::now()).count();
        return res >= 0 ? res : 0;
    }

    return -1;
}

void MinHeapTimer::clear()
{
    nodes.clear();
    while(!heap.empty()) heap.pop();
}