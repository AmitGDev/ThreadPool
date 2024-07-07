

**ThreadPool v1.0.0**

A **Cross-Platform** Flexible Thread Pool for **C++20** applications **(Header-Only Class)**

**Author:** Amit Gefen

**License:** MIT License

<br>

**Overview**

- Provides a high-level interface for thread pool in **C++20** applications.
- Supports various Submit mechanisms, including lambdas, functors, plain function pointers, and member functions.
- Implements graceful shutdown to ensure pool threads are completed before destruction.

<br>

**Features**

- Flexible Submut Options:
  - Submit tasks with lambdas, functors, plain function pointers, and member functions.
  - Pass optional arguments to the Submit for custom data.
- Robust Error Handling:
  - Catches and logs potential errors during submit and execution.
- Thread Safety:
  - Designed for safe usage in multithreaded environments.
- Graceful Shutdown:
  - Ensures pool threads are completed before the ThreadPool is destroyed.

<br>

**Usage**

\- Include the header file:
```cpp
#include "ThreadPool.h"
```
\- Create a thread pool and submit tasks using the Submit method:
```cpp
static void Function(uint32_t i)
{
	// Do something
}


int main()
{
	ThreadPool pool(4); // For example, a 4 threads thread pool.
	std::vector<std::future<void>> futures;

	for (uint32_t i = 1; i <= 8; ++i) {
		futures.emplace_back(pool.Submit(Function, i)); // For example, 8 tasks.
	}

	for (auto& future : futures) {
		future.wait();
	}
}
```

<br>

**Usage Example**

See the **main.cpp** file for a comprehensive example demonstrating various scheduling scenarios. <br>
Demo: https://onlinegdb.com/2biPyuygN
