#ifndef _SIMPLETHREADPOOL_
#define _SIMPLETHREADPOOL_          // 一定要加这，避免多次导入后，重复定义

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

	// 保证线程安全，所以拷贝构造函数、拷贝复制运算符重载、移动构造函数、移动赋值运算符重载都禁用了
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

// 简答任务队列，线程池
// 提交任务：普通函数，匿名函数(lambda函数)、仿函数(重载()运算符的类或结构体)、类成员函数、std::function/std::packaged_task、std::bind等
// 返回值不同，参数列表不同
class SimpleThreadPool {
public:
	// 保证线程安全，所以拷贝构造函数、拷贝复制运算符重载、移动构造函数、移动赋值运算符重载都禁用了
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

	// 包装成不带参数的调用对用，同时外部能得到它的返回值
	// 可变模板参数
	template<typename Func, typename... Args>
	auto submitTask(Func &&func, Args... args) -> std::future<decltype(func(args...))> {

		// 1、传进来的函数有返回值，要得到它的返回值的类型（std::invoke_result是在c++17才有的）
		//using returnType = typename std::invoke_result<Func, Args...>::type;
		using returnType = decltype(func(args...));
		// using returnType = std::invoke_result_t<Func, Args...>;  // 这两行是等价的，源码里也是用的using invoke_result_t 去得到的
		/*
		// C++11或者C++14可以用这行代码替代（但说是std::invoke_result是更为通用和强大的工具，因为它基于std::invoke机制，支持更广泛的调用类型。如果处理复杂的调用表达式，特别是涉及到成员函数或成员变量的调用，那么std::invoke_result是更好的选择。）
		using returnType = typename std::result_of<Func(Args...)>::type;
		*/


		// 2、将传进来的函数连通参数，包装成一个无参的函数（使用std::forward保证传递类型跟原来一模一样，不会发生变化）
		std::function<returnType()> taskWrapper01 = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);

		// 3、这是为了拿到返回值,再打包了一次，用了std::packaged_task，需要头文件 <future>
		// 这里用智能指针是为了下一步打包的传递
		auto taskWrapper02 = std::make_shared<std::packaged_task<returnType()>>(taskWrapper01);

		// 4、再打包一次，抹除返回值，因为任务队列需要的是没有返回值的函数，这里用lambda函数取去执行，又没写return，如果直接push taskWrapper02,那它执行是有返回值的，wrapperFunction执行是没返回值的
		// 这里不能是值传递，用引用好像也不太行，下面的&i就不对，所以上面包装成指针，这里直接传递指针
		// 这里写 TaskType wrapperFunction 也是样的，下面定义了 TaskType
		// 不知这里为啥能用 TaskType ，其它地方lambda整的函数，只能用auto+后置返回类型这种（可能是这个是包装成函数，是没返回值的）
		std::function<void()> wrapperFunction = [taskWrapper02]() {
			(*taskWrapper02)();  // 这个函数执行虽然后返回值，但不处理，lambda又没写return，这里就包装成了无返回值了
		};

		this->m_TaskQueue.push(wrapperFunction);
		this->m_CV.notify_one();

		return taskWrapper02->get_future();
	}
	/*
	// 上面的 auto submitTask(Func &&func, Args... args) {} 函数可以指定后置返回类型，不是必须，放这里当学习
	auto submitTask(Func &&func, Args... args) -> std::future<typename std::invoke_result<Func, Args...>::type> {}
	*/

	// 析构清理工作
	~SimpleThreadPool() {
		// 命令行运行才看得到这里，不然system("pause")就卡住了，它也要最后才自动析构，如果它是在main函数中运行的话。
		std::cout << "析构开始运行" << std::endl;
		this->m_RuningStatus = false;
		this->m_CV.notify_all();
		// 这种去拿去线程对象，必须是 & 引用
		for (auto &t : m_Threads) {
			// joinable方法主要判断是否可以使用join方法或者detach方法，可以返回true，不可以返回false
			// 一个线程最多只能调用一次join或者detach，可以可以判一下
			if (t.joinable()) {
				t.join();
			}
		}
		std::cout << "析构结束" << std::endl;
	}
private:
	// 初始化时，通过传参创建了12个线程，每个线程都在while (this->m_RuningStatus) {}中执行，当从队列中拿的一个任务完成后，它又循环去取，拿下一个任务，就保证线程在池中持续运行；实现线程池在整个程序生命周期中不断地处理任务队列中的任务
	// 当任务队列为空后，这个线程就会卡在条件变量的.wait函数等待
	// 所以最后析构的时候，把m_RuningStatus设为false，同时唤醒所有等待线程，往下运行后，也会退出while循环，就达到了退出
	void initialize() {
		for (size_t i = 0; i < m_Threads.size(); ++i) {
			// 这是构建了一个lambda函数，没有return那种，然后放进vector中（必须是i，不能是&i,不然值不对）
			auto worker = [this, i]() {
				while (this->m_RuningStatus) {
					TaskType task;
					bool isSUccess = false;
					// 下面用{}，相当以可以圈定unique_lock能在这段代码运行完后，生命周期就结束了，就释放锁
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
			// 这里创建时，子线程就在运行了,运行的就是上面的lambda函数，可一开始m_TaskQueue是空的，就会阻塞
			this->m_Threads[i] = std::thread(worker);
		}
	}

private:
	using TaskType = std::function<void()>;    // <void()> 代表接受的函数返回值是void，()代表没有参数

	// 这里我写成的单文件，以后用还是分文件写，并把这用智能指针类型替代
	SafeQueue<TaskType> m_TaskQueue;     // 这就是在实例化对象了，所以用指针，只是声明 
	std::vector<std::thread> m_Threads;
	std::condition_variable m_CV;
	std::mutex m_Mutex;

	std::atomic<bool> m_RuningStatus;
};
#endif // _SIMPLETHREADPOOL_