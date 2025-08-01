#include <iostream>
#include "thread_pool.h"

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
    std::cout << "main start" << std::endl;
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        std::cout << "before pool.start" << std::endl;
        pool.start(4);
        std::cout << "after pool.start - Initial threads: 4" << std::endl;

        // std::this_thread::sleep_for(std::chrono::seconds(5));
        // pool.submitTask(std::make_shared<MyTask>());

        std::cout << "before submitTask" << std::endl;
        auto result1 = pool.submitTask(std::make_shared<MyTask>(0, 100));
        auto result2 = pool.submitTask(std::make_shared<MyTask>(101, 200));
        auto result3 = pool.submitTask(std::make_shared<MyTask>(201, 300));
        auto result4 = pool.submitTask(std::make_shared<MyTask>(301, 400));
        auto result5 = pool.submitTask(std::make_shared<MyTask>(401, 500));
        auto result6 = pool.submitTask(std::make_shared<MyTask>(501, 600));
        std::cout << "after submitTask - Should have 6 total threads now" << std::endl;

        std::cout << "before result.get()" << std::endl;
        std::cout << "Getting result1..." << std::endl;
        int sum1 = result1->get().cast<int>();
        std::cout << "Got result1: " << sum1 << std::endl;
        
        std::cout << "Getting result2..." << std::endl;
        int sum2 = result2->get().cast<int>();
        std::cout << "Got result2: " << sum2 << std::endl;
        
        std::cout << "Getting result3..." << std::endl;
        int sum3 = result3->get().cast<int>();
        std::cout << "Got result3: " << sum3 << std::endl;
        
        std::cout << "Getting result4..." << std::endl;
        int sum4 = result4->get().cast<int>();
        std::cout << "Got result4: " << sum4 << std::endl;
        
        std::cout << "Getting result5..." << std::endl;
        int sum5 = result5->get().cast<int>();
        std::cout << "Got result5: " << sum5 << std::endl;
        
        std::cout << "Getting result6..." << std::endl;
        int sum6 = result6->get().cast<int>();
        std::cout << "Got result6: " << sum6 << std::endl;
        
        std::cout << "after result.get()" << std::endl;

        // 等待一段时间，让多余的线程被回收
        std::cout << "Waiting for thread recycling..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 减少等待时间
        
        std::cout << "after sleep:" << std::endl;
        std::cout << "sum = sum1 + sum2 + sum3 + sum4 + sum5 + sum6 = " << sum1 + sum2 + sum3 + sum4 + sum5 + sum6 << std::endl;

        std::cout << "About to destroy ThreadPool..." << std::endl;
    } // ThreadPool在这里被销毁
    std::cout << "ThreadPool destroyed" << std::endl;

    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());

    // getchar();
    std::cout << "main end" << std::endl;
    return 0;
}