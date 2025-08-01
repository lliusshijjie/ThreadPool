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
    isPoolRunning = false;
    notEmpty.notify_all(); // 通知所有线程退出
    // 停止所有线程
    std::unique_lock<std::mutex> lock(taskQueueMutex);
    exitThreadPool.wait(lock, [this]() {
        return taskQueue.empty();
    });
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
            while (taskQueue.empty()) {
                if (PoolMode::MODE_CACHED == mode) {
                    // 等待任务，如果超时则检查是否需要回收线程
                    if (std::cv_status::timeout == notEmpty.wait_for(lock, std::chrono::seconds(1))) {
                        auto now = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if (duration.count() >= THREAD_MAX_IDLE_TIME && totalThreadSize > initialThreadSize) {
                            threads.erase(threadId);
                            totalThreadSize--;
                            idleThreadSize--;
                            std::cout << "Thread " << threadId << " recycled due to idle timeout" << std::endl;
                            return;
                        }
                    }
                } 
                else {
                    // 固定模式，直接等待任务
                    notEmpty.wait(lock, [this]() {
                        return taskQueue.size() > 0;
                    });
                }

                if (!isPoolRunning) {
                    threads.erase(threadId);
                    exitThreadPool.notify_all(); // 通知主线程退出
                    return;
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

    threads.erase(threadId); // 从线程列表中移除当前线程
    std::cout << "Thread " << threadId << " exiting." << std::endl;
    exitThreadPool.notify_all(); // 通知主线程退出
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
