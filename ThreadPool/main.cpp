// main.cpp : This file contains the 'main' function. Program execution begins
// and ends there.
//

#include <chrono>
#include <iostream>
#include <syncstream>

#include "ThreadPool.h"

namespace {
constexpr size_t kDefaultPoolSize = 4;
constexpr size_t kDefaultThreadCount = 8;
constexpr uint8_t kTwoDigits = 10;

}  // namespace

// Call: log() to reset origin.
static void log(uint32_t index = 0, const char* action = nullptr) {
  static uint64_t origin{0};

  auto now = std::chrono::system_clock::now();
  auto epoch = now.time_since_epoch();
  auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(epoch).count();

  if (action == nullptr) {
    origin = seconds;

    return;
  }

  std::osyncstream sync_cout(std::cout);
  sync_cout << (seconds - origin)
            << (seconds - origin < kTwoDigits ? "  " : " ") << "task " << index
            << " " << action << '\n';
}

// **** Callable function: ****

static void Test_CallableObject() {
  // Create a ThreadPool with 4 threads
  ThreadPool pool(kDefaultPoolSize);

  // Vector to store futures of tasks
  std::vector<std::future<void>> futures;

  // Submit 8 tasks to the ThreadPool
  for (uint32_t index = 1; index <= kDefaultThreadCount; ++index) {
    futures.emplace_back(pool.Submit([index] {
      // Simulate some task execution
      log(index, "started");
      std::this_thread::sleep_for(std::chrono::seconds(index));  // 1 - 8
      log(index, "stopped");
    }));
  }

  // Wait for all tasks to complete
  for (auto& future : futures) {
    future.wait();
  }
}

// **** Function: ****

static void Function(uint32_t index) {
  log(index, "started");
  std::this_thread::sleep_for(std::chrono::seconds(index));
  log(index, "stopped");
}

static void Test_Function() {
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
    log(index, "started");
    ++counter_;
    std::this_thread::sleep_for(std::chrono::seconds(index));
    log(index, "stopped");
  }

  std::atomic_int_least64_t counter_{0};
};

// **** Main: ****

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int {
  try {
    std::cout
        << "**** note: each task runs for a number of seconds equal to its "
           "id ****"
        << '\n';

    std::cout << "\n**** test callable object: ****" << '\n';
    log();
    Test_CallableObject();

    std::cout << "\n**** test function: ****" << '\n';
    log();
    Test_Function();

    std::cout << "\n**** test member-function: ****" << '\n';
    log();
    MyClass my_class;
    my_class.Test();

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Caught standard exception in main: " << ex.what() << '\n';
  } catch (...) {
    std::cerr << "Caught unknown exception in main\n";
  }
  return 1;
}