# Enhanced C++ Thread Pool

A high-performance, thread-safe C++ thread pool implementation with support for both fixed and cached modes.

## Features

- **Fixed Mode**: Fixed number of threads for predictable resource usage
- **Cached Mode**: Dynamic thread creation and recycling based on workload
- **Thread-safe Task Submission**: Safe concurrent task submission
- **Result Retrieval**: Asynchronous result retrieval with synchronization
- **Graceful Shutdown**: Waits for all tasks to complete before shutting down
- **Task Queue Management**: Configurable task queue limits
- **Thread Recycling**: Automatic idle thread cleanup in cached mode

## Key Improvements

### 1. Thread Safety
- Added thread pool state checking in `submitTask()`
- Prevents task submission after thread pool shutdown
- Safe thread exit mechanism

### 2. Graceful Shutdown
- Waits for all pending tasks to complete
- Proper thread cleanup and resource management
- Timeout protection for thread exit

### 3. Fixed Mode Enhancement
- Fixed mode threads now properly respond to shutdown signals
- Prevents deadlock during thread pool destruction

### 4. Task Completion Guarantee
- All submitted tasks are guaranteed to execute
- No task loss during shutdown
- Proper result synchronization

## Usage

### Basic Usage
```cpp
#include "thread_pool.h"

// Create thread pool
ThreadPool pool;
pool.setMode(PoolMode::MODE_FIXED);
pool.start(4); // Start with 4 threads

// Submit tasks
auto result = pool.submitTask(std::make_shared<MyTask>(0, 100));

// Get result
int value = result->get().cast<int>();
```

### Cached Mode
```cpp
ThreadPool pool;
pool.setMode(PoolMode::MODE_CACHED);
pool.setThreadSizeLimit(10); // Max 10 threads
pool.start(4); // Initial 4 threads

// Threads will be created dynamically as needed
// Idle threads will be recycled after 60 seconds
```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Testing

Run the test executable:
```bash
./thread_pool_test
```

The test demonstrates both fixed and cached modes with various task scenarios.

## Architecture

### Core Classes
- **ThreadPool**: Main thread pool implementation
- **Task**: Abstract base class for user tasks
- **Result**: Asynchronous result container
- **Thread**: Individual thread wrapper
- **Any**: Type-safe value container
- **Semaphore**: Thread synchronization primitive

### Thread Management
- **Fixed Mode**: Maintains constant thread count
- **Cached Mode**: Dynamically creates/recycles threads
- **Idle Timeout**: 60-second timeout for thread recycling
- **Thread Exit**: Proper cleanup and resource release

## Thread Safety Features

1. **Atomic Counters**: Thread and task counters are atomic
2. **Mutex Protection**: Task queue access is synchronized
3. **Condition Variables**: Efficient thread waiting and notification
4. **State Checking**: Prevents operations on shutdown thread pool

## Performance Considerations

- **Task Queue**: Configurable size limits prevent memory issues
- **Thread Recycling**: Automatic cleanup reduces resource usage
- **Efficient Waiting**: Condition variables for optimal thread wakeup
- **Memory Management**: RAII principles for automatic cleanup

## License

This project is open source and available under the MIT License. 