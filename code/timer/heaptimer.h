#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack; //定义了一个名为TimeoutCallBack的函数类型别名，该函数类型别名可以用于表示一个无返回值、无参数的函数，可以用于存储任何可调用对象，包括函数指针、函数对象、lambda表达式
typedef std::chrono::high_resolution_clock Clock; //时钟类型，测量事件间隔和计算程序运行时间
typedef std::chrono::milliseconds MS; //时间间隔类型，用于计算程序运行时间
typedef Clock::time_point TimeStamp; //时钟的时间点，用于记录事件发生的时间

struct TimerNode {
    int id;
    TimeStamp expires;  // 超时时间点
    TimeoutCallBack cb; // 回调function<void()>
    bool operator<(const TimerNode& t) {    // 重载比较运算符
        return expires < t.expires;
    }
    bool operator>(const TimerNode& t) {    // 重载比较运算符
        return expires > t.expires;
    }
};
class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }  // 保留（扩充）容量
    ~HeapTimer() { clear(); }
    
    void adjust(int id, int newExpires);
    void add(int id, int timeOut, const TimeoutCallBack& cb);  //添加一个定时器
    void doWork(int id);
    void clear();
    void tick();
    void pop();
    int GetNextTick();

private:
    void del_(size_t i);    //删除指定定时器
    void siftup_(size_t i);     //向上调整
    bool siftdown_(size_t i, size_t n);     //向下调整，若不能向下则返回false
    void SwapNode_(size_t i, size_t j);     //交换两个结点位置

    std::vector<TimerNode> heap_;
    // key:id value:vector的下标
    std::unordered_map<int, size_t> ref_;   // id对应的在heap_中的下标，方便用heap_的时候查找
};

#endif //HEAP_TIMER_H