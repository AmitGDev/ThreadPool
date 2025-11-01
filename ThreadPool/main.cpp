// main.cpp : Program execution begins and ends there.
//

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iostream>
#include <syncstream>
#include <thread>
#include <vector>

#include "ThreadPool.h"

namespace {
constexpr size_t kDefaultPoolSize = 4;
constexpr size_t kDefaultThreadCount = 8;
constexpr uint8_t kTwoDigits = 10;

// Call: Log() to reset origin.
void Log(uint32_t index = 0, const char* action = nullptr) {
  static uint64_t origin{0};

  auto now = std::chrono::system_clock::now();
  auto epoch = now.time_since_epoch();
  auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(epoch).count();

  if (action == nullptr) {  // Reset origin
    origin = seconds;

    return;
  }

  std::osyncstream sync_cout(std::cout);
  sync_cout << (seconds - origin)
            << (seconds - origin < kTwoDigits ? "  " : " ") << "task " << index
            << " " << action << '\n';
}

// **** Callable function: ****

void TestCallableObject() {
  // Create a ThreadPool with 4 threads
  ThreadPool pool(kDefaultPoolSize);

  // Vector to store futures of tasks
  std::vector<std::future<void>> futures;

  // Submit 8 tasks to the ThreadPool
  for (uint32_t index = 1; index <= kDefaultThreadCount; ++index) {
    futures.emplace_back(pool.Submit([index] {
      // Simulate some task execution
      Log(index, "started");
      std::this_thread::sleep_for(std::chrono::seconds(index));  // 1 - 8
      Log(index, "stopped");
    }));
  }

  // Wait for all tasks to complete
  for (auto& future : futures) {
    future.wait();
  }
}

// **** Function: ****

void Function(uint32_t index) {
  Log(index, "started");
  std::this_thread::sleep_for(std::chrono::seconds(index));
  Log(index, "stopped");
}

void TestFunction() {
  ThreadPool pool(kDefaultPoolSize);
  std::vector<std::future<void>> futures;

  for (uint32_t index = 1; index <= kDefaultThreadCount; ++index) {
    futures.emplace_back(pool.Submit(Function, index));
  }

  for (auto& future : futures) {
    future.wait();
  }
}

// **** Member Function: ****

class MyClass final {
 public:
  void Test() {
    ThreadPool pool(kDefaultPoolSize);
    std::vector<std::future<void>> futures;

    for (uint32_t index = 1; index <= kDefaultThreadCount; ++index) {
      futures.emplace_back(pool.Submit(&MyClass::MemberFunction, this, index));
    }

    for (auto& future : futures) {
      future.wait();
    }
  }

 private:
  void MemberFunction(uint32_t index) {
    Log(index, "started");
    ++counter_;
    std::this_thread::sleep_for(std::chrono::seconds(index));
    Log(index, "stopped");
  }

  std::atomic_int_least64_t counter_{0};
};

}  // namespace

// **** Main: ****

auto main() -> int {  // NOLINT(bugprone-exception-escape)
  std::cout << "**** note: each task runs for a number of seconds equal to its "
               "id ****"
            << '\n';

  std::cout << "\n**** test callable object: ****" << '\n';
  Log();
  TestCallableObject();

  std::cout << "\n**** test function: ****" << '\n';
  Log();
  TestFunction();

  std::cout << "\n**** test member-function: ****" << '\n';
  Log();
  MyClass my_class;
  my_class.Test();
}