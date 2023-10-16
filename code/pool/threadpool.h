#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>


class ThreadPool {
public:
    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    // explicit 关键字声明类的构造函数是显式调用的，而非隐式调用
    explicit ThreadPool(int threadCount = 8) : pool_(std::make_shared<Pool>()) { 
        assert(threadCount > 0);
        for(int i = 0; i < threadCount; i++) {
            // 创建一个新的线程，并立即将其分离，意味着主线程不需要等待这个新线程结束
            // 新线程运行的是一个匿名函数（lambda表达式），通过捕获this指针来访问ThreadPool类的成员
            std::thread([this]() {
                std::unique_lock<std::mutex> locker(pool_->mtx_);
                while(true) {
                    // 如果任务队列不为空
                    if(!pool_->tasks.empty()) {
                        // 取出一个任务
                        auto task = std::move(pool_->tasks.front());    // 左值变右值,资产转移
                        pool_->tasks.pop();
                        locker.unlock(); //取出任务，提前解锁
                        task(); // 执行任务。此时可以解锁，允许其他线程访问任务队列
                        locker.lock(); //执行完任务，再次锁定
                    } else if(pool_->isClosed) {// 如果线程池被关闭
                        break;
                    } else {// 如果任务队列为空，线程池也没有被关闭
                        pool_->cond_.wait(locker);    //等待时会自动解锁互斥锁，允许其他线程访问任务队列。当有新的任务被添加到队列时，线程会被唤醒，并自动重新锁定互斥锁。
                    }
                    
                }
            }).detach();    //调用后和主线程分离，子线程结束时自己立即回收资源
        }
    }

    ~ThreadPool() {
        if(pool_) {
            std::unique_lock<std::mutex> locker(pool_->mtx_);
            pool_->isClosed = true;
        }
        pool_->cond_.notify_all();  // 唤醒所有的线程
    }

    template<typename T>
    void AddTask(T&& task) {
        std::unique_lock<std::mutex> locker(pool_->mtx_);
        pool_->tasks.emplace(std::forward<T>(task));
        pool_->cond_.notify_one();
    }

private:
    // 用一个结构体封装起来，方便调用
    struct Pool {
        std::mutex mtx_;    // 互斥锁
        std::condition_variable cond_;  //条件变量
        bool isClosed;  //是否关闭
        std::queue<std::function<void()>> tasks; // 任务队列，函数类型为void()
    };
    std::shared_ptr<Pool> pool_;
};

#endif