#ifndef AMITG_FC_THREADPOOL
#define AMITG_FC_THREADPOOL

/*
  ThreadPool.h
  Copyright (c) 2024, Amit Gefen

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

// ThreadPool class manages a pool of threads for task execution
class ThreadPool final {
 public:
  // Constructor to initialize the thread pool with a specified number of
  // threads
  explicit ThreadPool(size_t n);

  // Destructor to clean up the thread pool
  ~ThreadPool() noexcept;

  // Disallow copy and move operations for ThreadPool:
  ThreadPool(const ThreadPool&) = delete;  // Copy constructor.
  auto operator=(const ThreadPool&) -> ThreadPool& = delete; // Copy assignment operator.
  ThreadPool(ThreadPool&&) = delete;  // Move constructor.
  auto operator=(ThreadPool&&)
      -> ThreadPool& = delete;  // Move assignment operator.

  // 1) Submit a task, a callable (or function) with arguments to the thread
  // pool
  template <typename Callable, typename... Args>
  [[nodiscard]] auto Submit(Callable&& callable, Args&&... args);

  // 2) Submit a task, a member function with instance and arguments to the
  // thread pool
  template <typename MemberFunction, typename Instance, typename... Args>
  [[nodiscard]] auto Submit(MemberFunction&& member_function,
                            Instance&& instance, Args&&... args);

 private:
  // Worker function for the threads
  void Worker() noexcept;

  std::vector<std::jthread> workers_;        // Vector to hold worker threads
  std::queue<std::function<void()>> tasks_;  // Queue to hold tasks

  mutable std::mutex mutex_;    // Mutex for synchronizing access to the queue.
                                // mutable - M&M rule
  std::condition_variable cv_;  // Condition variable for task scheduling
  bool stop_{false};            // Flag to stop the thread pool
};

// Constructor
//
// - Starts n worker threads
//
// Throws:
//   - Any standard exceptions that might occur during thread creation.
ThreadPool::ThreadPool(const size_t n) {
  workers_.reserve(n);

  for (size_t i{0}; i < n; ++i) {
    workers_.emplace_back([this] { Worker(); });
  }
}

// Destructor
//
// - Gracefully stops the Worker objects.
// - Waits for the worker threads to finish execution using `join()`.
ThreadPool::~ThreadPool() noexcept {
  {
    std::lock_guard lock(mutex_);
    stop_ = true;
  }

  cv_.notify_all();  // Notify all threads to wake up

  // No need to call join() on each jthread in workers_, as jthread handles it
  // automatically.
}

// Worker function that runs in each thread
void ThreadPool::Worker() noexcept {
  while (true) {
    std::function<void()> task;

    {
      std::unique_lock lock(mutex_);
      // ACQUIRE: The mutex is locked here.

      // (I) Determine the predicate & (II) Decide whether to wait for new
      // tasks: (true? cv_wait doesn't block and doesn't unlock the mutex!
      // false? RELEASE & SLEEP, ATOMICALLY.)
      cv_.wait(lock, [this] {
        return stop_ || !tasks_.empty();
      });  // During SLEEP, when notified, if the predicate is true, ACQUIRE &
           // WAKE-UP, ATOMICALLY.

      if (stop_ && tasks_.empty()) {
        return;  // Exit (& RELEASE) if stopping and no tasks are left.
      }

      // Retrieves a stored lambda from the queue.
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    // RELEASE: The mutex is unlocked here.

    try {
      // When the lambda is executed, it calls the std::packaged_task, which
      // handles the return type and sets the result in the associated
      // std::promise.
      task();
    } catch (const std::exception& e) {
      std::cerr << "caught exception: " << e.what() << '\n';
    }
  }
}

// (1) Submit(callable, args...)
//
// Submit a task with the most flexible option, accepting any callable object.
// - `callable`: The lambda, functor, or function to be invoked.
// - `args...`: Optional arguments to be passed to the callable.
//
// Throws:
//   - `std::runtime_error` if the ThreadPool is stopped.
//   - Exceptions thrown by the callable are captured and stored in the returned
//   future.
template <typename Callable, typename... Args>
auto ThreadPool::Submit(Callable&& callable, Args&&... args) {
  // ReturnType Deduction
  using ReturnType = std::invoke_result_t<Callable, Args...>;

  // Create the Packaged Task. What does it pack?
  // a) `callable` (forwarded as an rvalue reference).
  // b) `args...` (forwarded as an rvalue reference).
  // This allows the callable to be executed asynchronously and provides a
  // `std::future` to retrieve the result.
  const auto packaged_task = std::make_shared<std::packaged_task<ReturnType()>>(
      [lambda_callable = std::forward<Callable>(callable),
       ... lambda_args = std::forward<Args>(
           args)]() mutable {  // mutable - Allow modification of captured
                               // variables inside the lambda.
        return lambda_callable(std::forward<Args>(lambda_args)...);
      });

  auto result = packaged_task->get_future();

  {
    std::lock_guard lock(mutex_);

    if (stop_) {
      throw std::runtime_error(
          "submit on stopped ThreadPool");  // Error if the pool is stopped
    }

    // Wrap & Store:
    // * Wrap `std::packaged_task` in a lambda that matches the
    // `std::function<void()>` signature. The lambda fits this signature because
    // it doesn�t return anything directly; it simply invokes the
    // `std::packaged_task`, which internally manages the return type and sets
    // the value in the `std::future`.
    // * Store that lambda in the `tasks_` queue:
    tasks_.emplace([lambda_task = std::move(
                        packaged_task)]() {  // Ensure the `packaged_task` is
                                             // moved rather than copied.
      try {
        (*lambda_task)();
      } catch (const std::exception& e) {
        std::cerr << "caught exception: " << e.what() << '\n';
      }
    });
  }

  cv_.notify_one();  // Notify one thread to process the task

  return result;  // Return the future
}

// (2) Submit(member_function, instance, args...)
//
// The purpose of this overload is to provide a convenient way to submit tasks
// that involve invoking a member function on an object.
// - `member_function`: A reference to the member function that you want to
// invoke.
// - `instance`: A reference to the object (instance) on which the member
// function will be called.
// - `args...`: Optional arguments to be passed to the member function.
//
// Throws:
//   - See overload (1)
template <typename MemberFunction, typename Instance, typename... Args>
auto ThreadPool::Submit(MemberFunction&& member_function, Instance&& instance,
                        Args&&... args) {
  // Create the Lambda Wrapper (a callable object). What does it wrap?
  // a) `member_function` (forwarded as an rvalue reference).
  // b) `instance` (forwarded as an rvalue reference).
  // c) `args...` (forwarded as a tuple).
  // When a task submitted via this overload is executed (later, within the
  // thread pool), the lambda is invoked.
  auto lambda_wrapper =
      [lambda_member_function = std::forward<MemberFunction>(member_function),
       lambda_instance = std::forward<Instance>(instance),
       lambda_args = std::make_tuple(std::forward<Args>(
           args)...)]() mutable {  // mutable - Allow modification of captured
                                   // variables inside the lambda
        // `std::apply` allows invoking a callable (such as a function or member
        // function) with a set of arguments provided as a tuple (Since C++17).
        // It unpacks the arguments from the tuple and calls the member function
        // with the specified instance and arguments. `std::tuple_cat` combines
        // the `lambda_instance` (wrapped in a single-element tuple) with the
        // `lambda_args`.
        return std::apply(
            lambda_member_function,
            std::tuple_cat(std::make_tuple(lambda_instance), lambda_args));
      };

  // Delegate to the callable object Submit (overload (1))
  return Submit(std::move(lambda_wrapper));  // Ensure the `lambda_wrapper` is
                                             // moved rather than copied.
}

#endif  // AMITG_FC_THREADPOOL
