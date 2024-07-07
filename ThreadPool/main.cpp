// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <syncstream>
#include <iostream>
#include "ThreadPool.h"


// Call: log() to reset origin.
static void log(uint32_t i = 0, const char* action = nullptr)
{
    static uint64_t origin{ 0 };

    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();

    if (action == nullptr) {
        origin = seconds;

        return;
    }

    std::osyncstream sync_cout(std::cout);
    sync_cout << (seconds - origin) << (seconds - origin < 10 ? "  " : " ") << "task " << i << " " << action << std::endl;
}


// **** Callable function: ****


static void Test_CallableObject()
{
    // Create a ThreadPool with 4 threads
    ThreadPool pool(4);

    // Vector to store futures of tasks
    std::vector<std::future<void>> futures;

    // Submit 8 tasks to the ThreadPool
    for (uint32_t i = 1; i <= 8; ++i) {
        futures.emplace_back(pool.Submit([i] {
            // Simulate some task execution
            log(i, "started");
            std::this_thread::sleep_for(std::chrono::seconds(i)); // 1 - 8
            log(i, "stopped");
            }));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.wait();
    }
}


// **** Function: ****


static void Function(uint32_t i)
{
    log(i, "started");
    std::this_thread::sleep_for(std::chrono::seconds(i));
    log(i, "stopped");
}


static void Test_Function()
{
    ThreadPool pool(4);
    std::vector<std::future<void>> futures;

    for (uint32_t i = 1; i <= 8; ++i) {
        futures.emplace_back(pool.Submit(Function, i));
    }

    for (auto& future : futures) {
        future.wait();
    }
}


// **** Member Function: ****


class MyClass final
{
public:
    void Test()
    {
        ThreadPool pool(4);
        std::vector<std::future<void>> futures;

        for (uint32_t i = 1; i <= 8; ++i) {
            futures.emplace_back(pool.Submit(&MyClass::MemberFunction, this, i));
        }

        for (auto& future : futures) {
            future.wait();
        }
    }

private:
    void MemberFunction(uint32_t i)
    {
        log(i, "started");
        std::this_thread::sleep_for(std::chrono::seconds(i));
        log(i, "stopped");
    }
};


// **** Main: ****


int main()
{
    std::cout << "**** note: each task runs for a number of seconds equal to its id ****" << std::endl;

    std::cout << "\n**** test callable object: ****" << std::endl;
    log();
    Test_CallableObject();

    std::cout << "\n**** test function: ****" << std::endl;
    log();
    Test_Function();

    std::cout << "\n**** test member-function: ****" << std::endl;
    log();
    MyClass my_class;
    my_class.Test();
}