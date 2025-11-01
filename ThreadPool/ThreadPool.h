// NOLINTBEGIN(llvm-header-guard)
#ifndef AMITG_FC_THREADPOOL_H_
#define AMITG_FC_THREADPOOL_H_
// NOLINTEND(llvm-header-guard)

/*
  ThreadPool.h
  Copyright (c) 2024-2025, Amit Gefen

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

#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <print>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// ThreadPool class manages a pool of threads for task execution
class ThreadPool final {
 public:
  explicit ThreadPool(std::size_t pool_size);
  ~ThreadPool() noexcept;

  // Non-copyable, non-moveable 
  ThreadPool(const ThreadPool&) = delete;
  auto operator=(const ThreadPool&) -> ThreadPool& = delete;
  ThreadPool(ThreadPool&&) = delete;
  auto operator=(ThreadPool&&) -> ThreadPool& = delete;

  // 1) Submit a callable with arguments.
  //    Tasks can be lambdas, functors, or free functions.
  template <std::invocable Callable, class... Args>
  [[nodiscard]] auto Submit(Callable&& callable, Args&&... args);

  // 2) Submit a member function with an instance and arguments.
  //    Convenience overload for object-oriented task dispatch.
  template <class MemberFunction, class Instance, class... Args>
    requires std::invocable<MemberFunction, Instance, Args...>
  [[nodiscard]] auto Submit(MemberFunction&& member_function,
                            Instance&& instance, Args&&... args);

 private:
  void Worker();

  std::vector<std::jthread> workers_;                  // worker threads
  std::queue<std::move_only_function<void()>> tasks_;  // task queue

  mutable std::mutex tasks_mutex_;
  std::condition_variable cv_;
  bool stop_{false};  // guarded by tasks_mutex_
};

// Constructor
//
// - Starts pool_size worker threads.
// - Each worker runs the internal Worker() loop.
// - Throwing here would likely mean thread creation failure.
inline ThreadPool::ThreadPool(const std::size_t pool_size) {
  workers_.reserve(pool_size);
  for (std::size_t index = 0; index < pool_size; ++index) {
    workers_.emplace_back([this] { Worker(); });
  }
}

// Destructor
//
// - Signals all workers to stop.
// - The order of operations ensures no lost wakeups:
//   lock -> set stop_ -> notify_all().
// - std::jthread ensures join() on destruction; no explicit join needed.
inline ThreadPool::~ThreadPool() noexcept {
  {
    const std::lock_guard lock(tasks_mutex_);
    stop_ = true;
  }
  cv_.notify_all();
}

// Worker
//
// - Waits on the condition variable for new tasks.
// - Wakes up when either new work arrives or stop_ is signaled.
// - Uses standard wait predicate pattern for atomic unlock+wait.
// - Exception isolation per task: a single bad task never kills the thread.
inline void ThreadPool::Worker() {
  while (true) {
    std::move_only_function<void()> task;

    {
      std::unique_lock lock(tasks_mutex_);
      cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

      if (stop_ && tasks_.empty()) {
        return;
      }

      task = std::move(tasks_.front());
      tasks_.pop();
    }

    try {
      task();
    } catch (const std::exception& ex) {
      std::print(stderr, "caught exception: {}\n", ex.what());
    } catch (...) {
      std::print(stderr, "caught unknown exception\n");
    }
  }
}

// (1) Submit(callable, args...)
//
// - Packages the callable + args into a std::packaged_task.
// - Pushes a void() lambda wrapper into the queue.
// - The future allows result retrieval.
// - stop_ check is under lock, consistent with destruction semantics.
template <std::invocable Callable, class... Args>
auto ThreadPool::Submit(Callable&& callable, Args&&... args) {
  using ReturnType = std::invoke_result_t<Callable, Args...>;

  const auto packaged_task = std::make_shared<std::packaged_task<ReturnType()>>(
      [lambda_callable = std::forward<Callable>(callable),
       ... lambda_args = std::forward<Args>(args)]() mutable {
        return std::invoke(std::move(lambda_callable),
                           std::move(lambda_args)...);
      });

  auto result = packaged_task->get_future();

  {
    const std::lock_guard lock(tasks_mutex_);
    if (stop_) {
      throw std::runtime_error("submit() on stopped ThreadPool");
    }

    tasks_.emplace([lambda_task = std::move(packaged_task)]() mutable {
      try {
        (*lambda_task)();
      } catch (const std::exception& ex) {
        std::print(stderr, "caught exception: {}\n", ex.what());
      } catch (...) {
        std::print(stderr, "caught unknown exception\n");
      }
    });
  }

  cv_.notify_one();
  return result;
}

// (2) Submit(member_function, instance, args...)
//
// - Provides syntactic sugar for calling member functions asynchronously.
// - Uses std::apply() + tuple_cat() to unpack instance and arguments.
// - Returns a future for the member function result.
template <class MemberFunction, class Instance, class... Args>
  requires std::invocable<MemberFunction, Instance, Args...>
auto ThreadPool::Submit(MemberFunction&& member_function, Instance&& instance,
                        Args&&... args) {
  auto lambda_wrapper =
      [lambda_member_function = std::forward<MemberFunction>(member_function),
       lambda_instance = std::forward<Instance>(instance),
       lambda_args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        return std::apply(
            lambda_member_function,
            std::tuple_cat(std::make_tuple(lambda_instance), lambda_args));
      };

  return Submit(std::move(lambda_wrapper));
}

#endif  // AMITG_FC_THREADPOOL_H_
