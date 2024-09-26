#ifndef _SIMPLETHREADPOOL_
#define _SIMPLETHREADPOOL_          // һ��Ҫ���⣬�����ε�����ظ�����

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

template<typename T>
class SafeQueue {
public:
	SafeQueue() {}
	~SafeQueue() {}

	// ��֤�̰߳�ȫ�����Կ������캯��������������������ء��ƶ����캯�����ƶ���ֵ��������ض�������
	SafeQueue(const SafeQueue &other) = delete;
	SafeQueue(SafeQueue &&other) = delete;
	SafeQueue(const SafeQueue &&other) = delete;

	SafeQueue& operator=(const SafeQueue &other) = delete;
	SafeQueue& operator=(const SafeQueue &&other) = delete;

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
	}

	void push(T&& value) {
		std::unique_lock<std::mutex> locker(this->m_Mutex);
		this->m_Queue.emplace(std::move(value));
	}

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

private:
	std::queue<T> m_Queue;
	std::mutex m_Mutex;
};

// ���������У��̳߳�
// �ύ������ͨ��������������(lambda����)���º���(����()����������ṹ��)�����Ա������std::function/std::packaged_task��std::bind��
// ����ֵ��ͬ�������б�ͬ
class SimpleThreadPool {
public:
	// ��֤�̰߳�ȫ�����Կ������캯��������������������ء��ƶ����캯�����ƶ���ֵ��������ض�������
	SimpleThreadPool(const SimpleThreadPool&) = delete;
	SimpleThreadPool(const SimpleThreadPool&&) = delete;

	SimpleThreadPool& operator=(const SimpleThreadPool&) = delete;
	SimpleThreadPool& operator=(const SimpleThreadPool&&) = delete;

	SimpleThreadPool() : m_Threads(std::thread::hardware_concurrency()), m_RuningStatus(true) {
		this->initialize();
	}
	SimpleThreadPool(int threadNum) : m_Threads(threadNum), m_RuningStatus(true) {
		this->initialize();
	}

	// ��װ�ɲ��������ĵ��ö��ã�ͬʱ�ⲿ�ܵõ����ķ���ֵ
	// �ɱ�ģ�����
	template<typename Func, typename... Args>
	auto submitTask(Func &&func, Args... args) -> std::future<decltype(func(args...))> {

		// 1���������ĺ����з���ֵ��Ҫ�õ����ķ���ֵ�����ͣ�std::invoke_result����c++17���еģ�
		//using returnType = typename std::invoke_result<Func, Args...>::type;
		using returnType = decltype(func(args...));
		// using returnType = std::invoke_result_t<Func, Args...>;  // �������ǵȼ۵ģ�Դ����Ҳ���õ�using invoke_result_t ȥ�õ���
		/*
		// C++11����C++14���������д����������˵��std::invoke_result�Ǹ�Ϊͨ�ú�ǿ��Ĺ��ߣ���Ϊ������std::invoke���ƣ�֧�ָ��㷺�ĵ������͡���������ӵĵ��ñ��ʽ���ر����漰����Ա�������Ա�����ĵ��ã���ôstd::invoke_result�Ǹ��õ�ѡ�񡣣�
		using returnType = typename std::result_of<Func(Args...)>::type;
		*/


		// 2�����������ĺ�����ͨ��������װ��һ���޲εĺ�����ʹ��std::forward��֤�������͸�ԭ��һģһ�������ᷢ���仯��
		std::function<returnType()> taskWrapper01 = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);

		// 3������Ϊ���õ�����ֵ,�ٴ����һ�Σ�����std::packaged_task����Ҫͷ�ļ� <future>
		// ����������ָ����Ϊ����һ������Ĵ���
		auto taskWrapper02 = std::make_shared<std::packaged_task<returnType()>>(taskWrapper01);

		// 4���ٴ��һ�Σ�Ĩ������ֵ����Ϊ���������Ҫ����û�з���ֵ�ĺ�����������lambda����ȡȥִ�У���ûдreturn�����ֱ��push taskWrapper02,����ִ�����з���ֵ�ģ�wrapperFunctionִ����û����ֵ��
		// ���ﲻ����ֵ���ݣ������ú���Ҳ��̫�У������&i�Ͳ��ԣ����������װ��ָ�룬����ֱ�Ӵ���ָ��
		// ����д TaskType wrapperFunction Ҳ�����ģ����涨���� TaskType
		// ��֪����Ϊɶ���� TaskType �������ط�lambda���ĺ�����ֻ����auto+���÷����������֣�����������ǰ�װ�ɺ�������û����ֵ�ģ�
		std::function<void()> wrapperFunction = [taskWrapper02]() {
			(*taskWrapper02)();  // �������ִ����Ȼ�󷵻�ֵ����������lambda��ûдreturn������Ͱ�װ�����޷���ֵ��
		};

		this->m_TaskQueue.push(wrapperFunction);
		this->m_CV.notify_one();

		return taskWrapper02->get_future();
	}
	/*
	// ����� auto submitTask(Func &&func, Args... args) {} ��������ָ�����÷������ͣ����Ǳ��룬�����ﵱѧϰ
	auto submitTask(Func &&func, Args... args) -> std::future<typename std::invoke_result<Func, Args...>::type> {}
	*/

	// ����������
	~SimpleThreadPool() {
		// ���������вſ��õ������Ȼsystem("pause")�Ϳ�ס�ˣ���ҲҪ�����Զ����������������main���������еĻ���
		std::cout << "������ʼ����" << std::endl;
		this->m_RuningStatus = false;
		this->m_CV.notify_all();
		// ����ȥ��ȥ�̶߳��󣬱����� & ����
		for (auto &t : m_Threads) {
			// joinable������Ҫ�ж��Ƿ����ʹ��join��������detach���������Է���true�������Է���false
			// һ���߳����ֻ�ܵ���һ��join����detach�����Կ�����һ��
			if (t.joinable()) {
				t.join();
			}
		}
		std::cout << "��������" << std::endl;
	}
private:
	// ��ʼ��ʱ��ͨ�����δ�����12���̣߳�ÿ���̶߳���while (this->m_RuningStatus) {}��ִ�У����Ӷ������õ�һ��������ɺ�����ѭ��ȥȡ������һ�����񣬾ͱ�֤�߳��ڳ��г������У�ʵ���̳߳��������������������в��ϵش�����������е�����
	// ���������Ϊ�պ�����߳̾ͻῨ������������.wait�����ȴ�
	// �������������ʱ�򣬰�m_RuningStatus��Ϊfalse��ͬʱ�������еȴ��̣߳��������к�Ҳ���˳�whileѭ�����ʹﵽ���˳�
	void initialize() {
		for (size_t i = 0; i < m_Threads.size(); ++i) {
			// ���ǹ�����һ��lambda������û��return���֣�Ȼ��Ž�vector�У�������i��������&i,��Ȼֵ���ԣ�
			auto worker = [this, i]() {
				while (this->m_RuningStatus) {
					TaskType task;
					bool isSUccess = false;
					// ������{}���൱�Կ���Ȧ��unique_lock������δ�����������������ھͽ����ˣ����ͷ���
					{
						std::unique_lock<std::mutex> locker(this->m_Mutex);
						if (this->m_TaskQueue.empty()) {
							this->m_CV.wait(locker);
						}
						isSUccess = this->m_TaskQueue.pop(task);
					}
					if (isSUccess) {
						//std::cout << "Start running task in worker:[ID] " << i << std::endl;
						task();
						//std::cout << "End running task in worker:[ID] " << i << std::endl;
					}
				}
			};
			// ���ﴴ��ʱ�����߳̾���������,���еľ��������lambda��������һ��ʼm_TaskQueue�ǿյģ��ͻ�����
			this->m_Threads[i] = std::thread(worker);
		}
	}

private:
	using TaskType = std::function<void()>;    // <void()> ������ܵĺ�������ֵ��void��()����û�в���

	// ������д�ɵĵ��ļ����Ժ��û��Ƿ��ļ�д��������������ָ���������
	SafeQueue<TaskType> m_TaskQueue;     // �������ʵ���������ˣ�������ָ�룬ֻ������ 
	std::vector<std::thread> m_Threads;
	std::condition_variable m_CV;
	std::mutex m_Mutex;

	std::atomic<bool> m_RuningStatus;
};
#endif // _SIMPLETHREADPOOL_