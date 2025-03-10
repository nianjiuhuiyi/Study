#ifndef _YOLOV5_SAFEQUEUE_H_
#define _YOLOV5_SAFEQUEUE_H_

#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class SafeQueue {
public:
    SafeQueue() {}
    ~SafeQueue() {}

    // 保证线程安全，所以靠包构造函数，重载的复制函数都禁用了
    SafeQueue(const SafeQueue &other) = delete;
    SafeQueue(SafeQueue &&other) = delete;
    SafeQueue(const SafeQueue &&other) = delete;

    SafeQueue &operator=(const SafeQueue &other) = delete;
    SafeQueue &operator=(const SafeQueue &&other) = delete;

    bool empty() {
        std::unique_lock<std::mutex> locker(this->m_Mutex);
        return this->m_Queue.empty();
    }

    int size() {
        std::unique_lock<std::mutex> locker(this->m_Mutex);
        return this->m_Queue.size();
    }

    void push(const T &value) {
        std::unique_lock<std::mutex> locker(this->m_Mutex);
        this->m_Queue.emplace(value);
        this->m_Cond.notify_one();
    }

    void push(T &&value) {
        std::unique_lock<std::mutex> locker(this->m_Mutex);
        this->m_Queue.emplace(std::move(value));
        this->m_Cond.notify_one();
    }

    /*
    // 这是比较符和安全的pop方法，我放这里，但我准备改成更符合我的使用习惯，也有点像python那种
    bool pop() {
        std::unique_lock<std::mutex> locker(this->m_Mutex);
        if (this->m_Queue.empty()) {
            return false;
        }
        this->m_Queue.pop();
        return true;
    }

    bool pop(T &value) {
        std::unique_lock<std::mutex> locker(this->m_Mutex);
        if (this->m_Queue.empty()) {
            return false;
        }
        value = std::move(this->m_Queue.front());
        this->m_Queue.pop();
        return true;
    }
    */

    // 与原始的queue队列使用保持一致，自己pop前先去判空
    void pop() {
        std::unique_lock<std::mutex> locker(this->m_Mutex);
        this->m_Queue.pop();
    }

    // 增加一个方法，类似于python那种, 当队列为空时，会阻塞在这里
    T get() {
        std::unique_lock<std::mutex> locker(this->m_Mutex);
        this->m_Cond.wait(locker, [&]() { return !this->m_Queue.empty(); });
        T value = this->m_Queue.back();
        this->m_Queue.pop();
        return value;
    }

    template <typename _Rep, typename _Period>
    T get(const std::chrono::duration<_Rep, _Period> &_Rel_time) {
        std::unique_lock<std::mutex> locker(this->m_Mutex);

        bool ret = this->m_Cond.wait_for(locker, _Rel_time, [&]() { return !this->m_Queue.empty(); });

        // 这里超时直接是抛出error了，可能vs运行时显示的是 “T value = this->m_Queue.back();”这行报错，跟它没关系，暂时就这样吧
        if (!ret) {
            // std::cout << "get等待超时了" << std::endl;
            throw std::runtime_error("超时了...");
        }

        T value = this->m_Queue.back();
        this->m_Queue.pop();
        return value;
    }

    T front() {
        std::unique_ptr<std::mutex> locker(this->m_Mutex);
        return this->m_Queue.front();
    }

    T back() {
        std::unique_ptr<std::mutex> locker(this->m_Mutex);
        return this->m_Queue.back();
    }

private:
    std::queue<T> m_Queue;
    std::mutex m_Mutex;
    std::condition_variable m_Cond;
};

#endif // _YOLOV5_SAFEQUEUE_H_
