#ifndef __THREAD_POOL_H
#define __THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <utility>
#include <unordered_map>
#include <thread>
#include <typeinfo>

// Any 类型：可以接受任意数据的类型
class Any {
public:
    Any() = default;
    ~Any() = default;

    // 禁止拷贝构造和赋值
    Any(const Any&) = delete;
    Any &operator = (const Any&) = delete;

    Any(Any&&) = default;
    Any &operator = (Any&&) = default;

    // 模板构造函数，接受任意类型的参数
    template<typename T>
    Any(T data) : m_base(std::make_unique<Derived<T>>(data)) {}

    // 获取存储的值
    template<typename T>
    T cast() {
        // 基类指针转换成派生类指针，使用 dynamic_cast 进行类型转换
        Derived<T>* ans = dynamic_cast<Derived<T>*>(m_base.get());
        if (!ans) {
            throw std::bad_cast();
        }
        return ans->get();
    }

private:
    // 基类，所有类型都继承自这个基类
    class Base {
    public:
        virtual ~Base() = default;
    }; 

    // 派生类
    template<typename T> 
    class Derived : public Base { 
    public:
        Derived(T data) : m_data(data) {}
        ~Derived() = default;
        T get() const {
            return m_data;
        }
    private:
        T m_data;
    };
private:
    std::unique_ptr<Base> m_base;
};

// 这里实现一个信号量类
class Semaphore {
public:
    Semaphore(int cnt = 0) : count(cnt) {}
    ~Semaphore() = default;

    // 获取一个信号量资源
    void wait() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() {
            return count > 0;
        });
        count--;       
    }

    // 增加一个信号量资源
    void post() {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
        cv.notify_all();
    }

private:
    int count; // 信号量计数器
    std::mutex mtx;
    std::condition_variable cv;
};

// 前向声明
class Task;

// 实现接收提交到线程池的task任务执行完成后的返回值Result
class Result {
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;

    // setValue 获取任务执行完的返回值
    void setValue(Any any);
    // get 用户调用这个方法获取task执行结果
    Any get();

private:
    Any m_any; // 存储任务的返回值
    Semaphore m_semaphore; // 线程通信信号量
    std::weak_ptr<Task> m_task; // 任务指针
    std::atomic<bool> m_isValid; // 任务是否执行完成
};

// 任务抽象基类
class Task {
public:
    Task();
    virtual ~Task() = default;

    void execute();
    void setResultPtr(std::shared_ptr<Result> resultPtr);
    // 纯虚函数，不能够使用模板
    virtual Any run() = 0;
private:
    std::shared_ptr<Result> m_resultPtr;
};

// 线程池支持的模式
enum class PoolMode {
    MODE_FIXED, // 固定线程数量
    MODE_CACHED // 可动态调整线程数量
};

// 线程类
class Thread {
public:
    using ThreadFunc = std::function<void(int)>;

    Thread(ThreadFunc func);
    ~Thread();

    // 启动线程
    void start();

    // 获取线程id
    int getThreadId() const;

private:
    ThreadFunc threadfunc;
    static int generateId;
    int threadId; //  线程id
};

// 线程池类
class ThreadPool {
public:
    ThreadPool(); 
    ~ThreadPool();
    ThreadPool(const ThreadPool& ) = delete;
    ThreadPool &operator= (const ThreadPool& ) = delete;

    // 设置线程池工作模式
    void setMode(PoolMode mode);

    // 设置taskQueue最大容纳任务数
    void setTaskQueueLimit(size_t size);
    // 设置Cached模式下的线程数量上限
    void setThreadSizeLimit(size_t size);
    // 给线程池提交任务
    std::shared_ptr<Result> submitTask(std::shared_ptr<Task> task);

    // 启动线程池
    void start(size_t initialThreadSize = 4);

private:
    // 线程函数
    void threadFunc(int threadId);

    // 检查pool的运行状态
    bool checkPoolRunning() const;

private:
    std::unordered_map<int, std::unique_ptr<Thread>> threads; // 线程列表
    size_t initialThreadSize;        // 初始线程数量
    std::atomic<int> idleThreadSize; // 表示当前线程池中空闲线程的数量
    std::atomic<int> totalThreadSize; // 表示当前线程池中线程的总数量

    std::queue<std::shared_ptr<Task>> taskQueue; // 任务队列
    std::atomic<size_t> taskSize; // 任务计数器
    size_t taskQueueLimit; // 任务队列大小上限
    size_t threadSizeLimit; // Cached模式下的线程数量上限

    std::mutex taskQueueMutex; // 任务队列互斥锁
    std::condition_variable notEmpty; // 任务队列非空条件变量
    std::condition_variable notFull; // 任务队列非满条件变量

    PoolMode mode; // 当前线程池模式
    std::atomic<bool> isPoolRunning; // 表示当前线程池的启动状态 
};

#endif
