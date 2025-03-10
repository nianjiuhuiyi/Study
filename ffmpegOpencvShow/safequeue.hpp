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

    // ��֤�̰߳�ȫ�����Կ������캯�������صĸ��ƺ�����������
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
    // ���ǱȽϷ��Ͱ�ȫ��pop�������ҷ��������׼���ĳɸ������ҵ�ʹ��ϰ�ߣ�Ҳ�е���python����
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

    // ��ԭʼ��queue����ʹ�ñ���һ�£��Լ�popǰ��ȥ�п�
    void pop() {
        std::unique_lock<std::mutex> locker(this->m_Mutex);
        this->m_Queue.pop();
    }

    // ����һ��������������python����, ������Ϊ��ʱ��������������
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

        // ���ﳬʱֱ�����׳�error�ˣ�����vs����ʱ��ʾ���� ��T value = this->m_Queue.back();�����б�������û��ϵ����ʱ��������
        if (!ret) {
            // std::cout << "get�ȴ���ʱ��" << std::endl;
            throw std::runtime_error("��ʱ��...");
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
