#include <iostream>
#include "thread_pool.h"
#include <numeric>

class MyTask : public Task {
public:
    // 问题一: 怎么设计run函数的返回值，可以表示任意的类型
    // Java Python 中的Object类型是所有类的基类
    // C++17 Any类型
    MyTask(int begin, int end) : m_begin(begin), m_end(end) {}
    Any run() override {
        std::cout << "MyTask running: " << m_begin << " to " << m_end << std::endl;
        int sum = 0;
        for(int i = m_begin; i <= m_end; i++) {
            sum += i;
        }
        std::cout << "MyTask finished: sum = " << sum << std::endl;
        return sum;
        // std::cout << "MyTask is running in thread: " 
        //           << std::this_thread::get_id() << std::endl;
        // std::this_thread::sleep_for(std::chrono::seconds(3));
        // std::cout << "MyTask is finished." << std::endl;
        // return 42; // 返回一个整数
    }
private:
    int m_begin, m_end;
};
 
int main() {
    #if 0
        {
            std::cout << "=== Testing Fixed Mode ===" << std::endl;
            ThreadPool pool;
            pool.setMode(PoolMode::MODE_FIXED);
            std::cout << "before pool.start" << std::endl;
            pool.start(4);
            std::cout << "after pool.start - Initial threads: 4" << std::endl;

            auto res1 = pool.submitTask(std::make_shared<MyTask>(0, 100));
            auto res2 = pool.submitTask(std::make_shared<MyTask>(101, 200));

            std::cout << "Task submitted, waiting for result..." << std::endl;

            int result1 = res1->get().cast<int>();
            int result2 = res2->get().cast<int>();
            std::cout << "Result = result1 + result2 = " << result1 + result2 << std::endl;

        }
        std::cout << "ThreadPool destroyed" << std::endl;
        std::cout << "=== Fixed Mode Test Complete ===" << std::endl;
    #else 
        // Cached模式测试 - 测试线程池动态扩展功能
        std::cout << "=== Testing Cached Mode ===" << std::endl;
        {
            ThreadPool pool;
            pool.setMode(PoolMode::MODE_CACHED);
            std::cout << "before pool.start" << std::endl;
            pool.start(4);
            std::cout << "after pool.start - Initial threads: 4" << std::endl;

            std::cout << "Submitting 10 tasks to test thread expansion..." << std::endl;
            int result = 0;
            std::vector<int> results;
            for (int i = 0; i < 10; i++) {
                pool.submitTask(std::make_shared<MyTask>(100 * i, 100 * (i + 1)));             
            }

            /*auto result1 = pool.submitTask(std::make_shared<MyTask>(0, 100));
            auto result2 = pool.submitTask(std::make_shared<MyTask>(101, 200));
            auto result3 = pool.submitTask(std::make_shared<MyTask>(201, 300));
            auto result4 = pool.submitTask(std::make_shared<MyTask>(301, 400));
            auto result5 = pool.submitTask(std::make_shared<MyTask>(401, 500));
            auto result6 = pool.submitTask(std::make_shared<MyTask>(501, 600));*/

            // std::cout << "All tasks submitted - Should have 10 total threads now" << std::endl;

            // std::cout << "Getting results..." << std::endl;      

            // // 等待一段时间，让多余的线程被回收
            std::cout << "Waiting for thread recycling (60s timeout)..." << std::endl;
            
            // std::cout << "Final calculation: " << std::endl;
            // std::cout << "final result = " << accumulate(results.begin(), results.end(), 0) << std::endl;

            std::cout << "About to destroy ThreadPool..." << std::endl;
        } // ThreadPool在这里被销毁
        std::cout << "ThreadPool destroyed" << std::endl;
        std::cout << "=== Cached Mode Test Complete ===" << std::endl;
    #endif
    std::cout << "main end" << std::endl;
    return 0;
}