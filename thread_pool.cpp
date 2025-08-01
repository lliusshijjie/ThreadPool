#include "thread_pool.h"
#include <functional>
#include <thread>
#include <iostream>
#include <chrono>

const size_t TASK_MAX_SIZE = 1024;
const size_t THREAD_MAX_SIZE = 10;
const size_t THREAD_MAX_IDLE_TIME = 60;
/*
    这里是线程池的实现代码
    线程池的主要功能是管理一组线程，并提供任务提交和执行的接口。
    线程池可以根据不同的模式（固定线程数量或可动态调整线程数量）来优化性能，以及根据业务需求动态调整线程池中线程的数量。
    线程池提供了任务执行、停止、重启、销毁和清理等基础功能。
*/

ThreadPool::ThreadPool() : initialThreadSize(0), taskSize(0), totalThreadSize(0),
                            taskQueueLimit(TASK_MAX_SIZE), mode(PoolMode::MODE_FIXED), 
                            isPoolRunning(false), idleThreadSize(0), threadSizeLimit(THREAD_MAX_SIZE)
                            {}

ThreadPool::~ThreadPool() {
    std::cout << "ThreadPool destructor called. Current thread count: " << threads.size() << std::endl;
    
    // 设置停止标志，停止接受新任务
    isPoolRunning = false;
    
    // 等待所有任务执行完成
    {
        std::unique_lock<std::mutex> lock(taskQueueMutex);
        while (taskSize > 0) {
            std::cout << "Waiting for " << taskSize << " tasks to complete..." << std::endl;
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            lock.lock();
        }
        std::cout << "All tasks completed." << std::endl;
    }
    
    // 通知所有等待的线程退出
    notEmpty.notify_all();
    notFull.notify_all();
    
    // 等待所有线程自然退出
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(3); // 3秒超时
    
    while (!threads.empty()) {
        auto now = std::chrono::steady_clock::now();
        if (now - startTime > timeout) {
            std::cout << "Timeout waiting for threads to exit, forcing cleanup" << std::endl;
            break;
        }
        
        // 短暂等待，让线程有机会自然退出
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 由于线程现在会自己从threads容器中删除，我们只需要等待
        // 线程退出时会自动更新计数器并从容器中删除自己
    }
    
    // 最终清理 - 现在应该很少需要强制清理了
    if (!threads.empty()) {
        std::cout << "Forcing cleanup of remaining threads: " << threads.size() << std::endl;
        for (auto it = threads.begin(); it != threads.end(); ++it) {
            std::cout << "Forcing cleanup of thread " << it->first << std::endl;
        }
        std::unique_lock<std::mutex> lock(taskQueueMutex);
        totalThreadSize = 0; // 重置线程总数
        idleThreadSize = 0; // 重置空闲线程数
        taskSize = 0; // 重置任务计数器       
        threads.clear();
    }
    
    std::cout << "All threads have exited. Final thread count: " << threads.size() << std::endl;
}

bool ThreadPool::checkPoolRunning() const {
    return isPoolRunning;
}

// 设置线程池工作模式
void ThreadPool::setMode(PoolMode mode) {
    if (checkPoolRunning()) return;
    this->mode = mode;
}

// 设置线程池初始线程数量
// void ThreadPool::setInitialThreadSize(size_t size) {
//     this->initialThreadSize = size;
// }

// 设置taskQueue最大容纳任务数
void ThreadPool::setTaskQueueLimit(size_t size) {
    if (checkPoolRunning()) return;
    if (PoolMode::MODE_CACHED == mode) {
        this->taskQueueLimit = size;
    }   
}

// 设置Cached模式下的线程数量上限
void ThreadPool::setThreadSizeLimit (size_t size) {
    this->threadSizeLimit = size;
}

// 给线程池提交任务(用户调用该接口，传入任务对象，生产任务)
std::shared_ptr<Result> ThreadPool::submitTask(std::shared_ptr<Task> task) {
    // 检查线程池是否还在运行
    if (!isPoolRunning) {
        std::cerr << "Task submission failed: thread pool is not running." << std::endl;
        return std::make_shared<Result>(task, false);
    }
    
    auto result = std::make_shared<Result>(task, true);
    task->setResultPtr(result);
    std::unique_lock<std::mutex> lock(taskQueueMutex);
    bool Ret = notFull.wait_for(lock, std::chrono::seconds(1), [this]() {
        return taskQueue.size() < taskQueueLimit;
    });
    if (!Ret) {
        std::cerr << "Task submission failed: taskqueue is full." << std::endl;
        return std::make_shared<Result>(task, false);
    } else {
        std::cout << "Task submitted successfully." << std::endl;
    }
    taskQueue.emplace(task);
    taskSize++;
    notEmpty.notify_all();

    // Cached 模式 任务处理比较紧急 场景：小而快的任务 
    // 需要根据任务数量和空闲线程的数量判断是否开启 Cached 模式
    if (mode == PoolMode::MODE_CACHED && taskSize > idleThreadSize && totalThreadSize < threadSizeLimit) {
        std::cout << " >>>> start a new thread. <<<< " << std::endl;
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        // threads.emplace_back(std::move(ptr));
        int id = ptr->getThreadId();
        threads.emplace(id, std::move(ptr));
        
        // 启动线程
        threads[id]->start(); 

        // 更新线程数量相关的值
        totalThreadSize++;
        idleThreadSize++;
    }

    return result;
}

// 启动线程池
void ThreadPool::start(size_t initialThreadSize) {
    isPoolRunning = true;
    this->initialThreadSize = initialThreadSize;
    this->totalThreadSize = initialThreadSize;
    for (int i = 0; i < initialThreadSize; i++)
    {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int id = ptr->getThreadId();
        threads.emplace(id, std::move(ptr));
        threads[id]->start();
        idleThreadSize++;
    }
}

// 定义线程函数 线程池的所有线程从任务队列里面消费任务
void ThreadPool::threadFunc(int threadId) {
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (isPoolRunning) {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(taskQueueMutex);
            
            // 在cached模式下，有可能已经创建了许多线程，但是空闲时间可能超过60s
            // 那么应该把多余的线程进行回收
            // 如果当前时间 - 上一次线程执行的时间 > 60s
            while (taskQueue.empty() && isPoolRunning) {
                if (PoolMode::MODE_CACHED == mode) {
                    // 等待任务，如果超时则检查是否需要回收线程
                    if (std::cv_status::timeout == notEmpty.wait_for(lock, std::chrono::seconds(1))) {
                        auto now = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if (duration.count() >= THREAD_MAX_IDLE_TIME && totalThreadSize > initialThreadSize) {
                            // 更新计数器
                            auto it = threads.find(threadId);
                            if (it != threads.end()) {
                                threads.erase(it);
                                idleThreadSize--;
                                totalThreadSize--;
                                std::cout << "Thread " << threadId << " recycled due to idle timeout" << std::endl;
                            }
                            return;
                        }
                    }
                } 
                else {
                    // 固定模式，等待任务或线程池关闭
                    notEmpty.wait(lock, [this]() {
                        return taskQueue.size() > 0 || !isPoolRunning;
                    });
                }

                if (!isPoolRunning) {
                    break; // 跳出while循环，执行最后的清理代码
                }

            }

            // 再次检查队列是否为空（防止竞态条件）
            if (taskQueue.empty()) {
                continue;
            }
 
            idleThreadSize--;
            task = std::move(taskQueue.front());
            taskQueue.pop();
            taskSize--;
            if (taskQueue.size() > 0) {
                notEmpty.notify_all();
            }
            notFull.notify_all();
        }

        if (task) {
            task->execute();
        } else {
            std::cerr << "taskQueue is null." << std::endl;
        }
        idleThreadSize++;
        lastTime = std::chrono::high_resolution_clock::now(); // 更新时间
    }

    // 线程退出时更新计数器
    auto it = threads.find(threadId);
    if (it != threads.end()) {
        threads.erase(it);
        totalThreadSize--;
        idleThreadSize--;
        std::cout << "Thread " << threadId << " exiting." << std::endl;
    }
}



/*
    这里是线程实现代码

*/

int Thread::generateId = 0;

Thread::Thread(ThreadFunc func) : threadfunc(func), threadId(generateId++) {}

Thread::~Thread() {}

// 启动线程
void Thread::start() {
    std::thread t(threadfunc, threadId);
    t.detach(); // 分离线程，使其在后台运行
}

int Thread::getThreadId() const {
    return threadId;
}

/*
    这里是Task实现代码
*/

Task::Task() {}

void Task::setResultPtr(std::shared_ptr<Result> resultPtr) {
    m_resultPtr = resultPtr;
}

void Task::execute() {
    if (m_resultPtr) {
        m_resultPtr->setValue(run());
    }
}


/*

    这里是Result实现代码  

*/

Result::Result(std::shared_ptr<Task> task, bool isValid) : m_task(task), m_isValid(isValid) {
    // 不再setResult
}

void Result::setValue(Any any) {
    // 存储task的返回值
    m_any = std::move(any);
    m_semaphore.post(); // 任务执行完成，通知等待的线程
}

Any Result::get() {
    if (!m_isValid) return "";
    m_semaphore.wait(); // 等待任务执行完成
    return std::move(m_any);
}
