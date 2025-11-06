#pragma once

#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <liburing.h>
#include <memory>
#include <queue>
#include <stdexcept>
#include <unordered_map>

#include "task.hh"

namespace async {

// Operation context - stores coroutine handle and result
struct OperationContext
{
  std::coroutine_handle<> coroutine;
  int32_t result { 0 }; // Will store bytes transferred or -errno
  bool completed { false };

  explicit OperationContext( std::coroutine_handle<> coro )
    : coroutine( coro )
  {}
};

// The io_uring runtime that manages the async event loop
class IoUringRuntime
{
public:
  // Initialize with specified queue depth (default 256)
  explicit IoUringRuntime( uint32_t queue_depth = 256 );

  ~IoUringRuntime();

  // Non-copyable, non-movable (manages kernel resources)
  IoUringRuntime( const IoUringRuntime& ) = delete;
  IoUringRuntime& operator=( const IoUringRuntime& ) = delete;
  IoUringRuntime( IoUringRuntime&& ) = delete;
  IoUringRuntime& operator=( IoUringRuntime&& ) = delete;

  // Main event loop - runs until all tasks complete
  void run();

  // Run until there are no more ready tasks
  void run_until_idle();

  // Submit a single SQE and return operation context
  OperationContext* submit_sqe( std::function<void( io_uring_sqe* )> prepare_sqe );

  // Schedule a task to run
  template<typename T>
  void spawn( Task<T>&& task )
  {
    auto handle = task.handle();
    task.detach();
    schedule( handle );
  }

  // Schedule a coroutine handle to resume
  void schedule( std::coroutine_handle<> handle );

  // Check if there are any pending operations or tasks
  bool has_pending_work() const noexcept;

  // Get the global runtime instance (thread-local)
  static IoUringRuntime* current() noexcept;

private:
  // Process one batch of completions
  void process_completions();

  // Submit all pending SQEs
  void submit();

  // Resume all ready coroutines
  void resume_ready_coroutines();

  io_uring ring_;
  std::queue<std::coroutine_handle<>> ready_queue_;
  std::unordered_map<uint64_t, std::unique_ptr<OperationContext>> pending_operations_;
  uint64_t next_operation_id_ { 1 }; // 0 is reserved
  bool running_ { false };

  // Thread-local current runtime
  static thread_local IoUringRuntime* current_runtime_;
};

// Get the current runtime (throws if not in runtime context)
inline IoUringRuntime& current_runtime()
{
  auto* runtime = IoUringRuntime::current();
  if ( !runtime ) {
    throw std::logic_error( "No active IoUringRuntime in current thread" );
  }
  return *runtime;
}

} // namespace async
