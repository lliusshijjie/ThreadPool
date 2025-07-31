#include <iostream>
#include "thread_pool.h"

class MyTask : public Task {
public:
    // 问题一: 怎么设计run函数的返回值，可以表示任意的类型
    // Java Python 中的Object类型是所有类的基类
    // C++17 Any类型
    MyTask(int begin, int end) : m_begin(begin), m_end(end) {}
    Any run() override {
        int sum = 0;
        for(int i = m_begin; i <= m_end; i++) {
            sum += i;
        }
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
    ThreadPool pool;
    pool.setMode(PoolMode::MODE_CACHED);
    std::cout << "before pool.start" << std::endl;
    pool.start(4);
    std::cout << "after pool.start" << std::endl;

    // std::this_thread::sleep_for(std::chrono::seconds(5));
    // pool.submitTask(std::make_shared<MyTask>());

    std::cout << "before submitTask" << std::endl;
    auto result1 = pool.submitTask(std::make_shared<MyTask>(0, 100));
    auto result2 = pool.submitTask(std::make_shared<MyTask>(101, 200));
    auto result3 = pool.submitTask(std::make_shared<MyTask>(201, 300));
    std::cout << "after submitTask" << std::endl;

    std::cout << "before result.get()" << std::endl;
    int sum1 = result1->get().cast<int>();
    int sum2 = result2->get().cast<int>();
    int sum3 = result3->get().cast<int>();
    std::cout << "after result.get()" << std::endl;

    // std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "sum = sum1 + sum2 + sum3 = " << sum1 + sum2 + sum3 << std::endl;

    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());

    // getchar();
    std::cout << "main end" << std::endl;
    return 0;
}