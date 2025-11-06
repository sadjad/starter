#include "io_uring_runtime.hh"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <system_error>

namespace async {

thread_local IoUringRuntime* IoUringRuntime::current_runtime_ = nullptr;

IoUringRuntime::IoUringRuntime( uint32_t queue_depth )
{
  // Initialize io_uring
  int ret = io_uring_queue_init( queue_depth, &ring_, 0 );
  if ( ret < 0 ) {
    throw std::system_error( -ret, std::system_category(), "io_uring_queue_init failed" );
  }

  // Set this as the current runtime for this thread
  current_runtime_ = this;
}

IoUringRuntime::~IoUringRuntime()
{
  // Clean up io_uring
  io_uring_queue_exit( &ring_ );

  // Clear current runtime
  if ( current_runtime_ == this ) {
    current_runtime_ = nullptr;
  }
}

void IoUringRuntime::run()
{
  running_ = true;

  while ( running_ && has_pending_work() ) {
    // Resume any ready coroutines
    resume_ready_coroutines();

    // Submit pending operations
    submit();

    // Wait for and process completions
    process_completions();
  }

  running_ = false;
}

void IoUringRuntime::run_until_idle()
{
  // Resume all ready coroutines
  resume_ready_coroutines();

  // Submit pending operations
  submit();

  // Process completions without blocking
  io_uring_cqe* cqe = nullptr;
  while ( io_uring_peek_cqe( &ring_, &cqe ) == 0 && cqe != nullptr ) {
    uint64_t operation_id = io_uring_cqe_get_data64( cqe );

    if ( operation_id != 0 ) {
      auto it = pending_operations_.find( operation_id );
      if ( it != pending_operations_.end() ) {
        auto& ctx = it->second;
        ctx->result = cqe->res;
        ctx->completed = true;

        // Schedule coroutine to resume
        if ( ctx->coroutine ) {
          ready_queue_.push( ctx->coroutine );
        }

        pending_operations_.erase( it );
      }
    }

    io_uring_cqe_seen( &ring_, cqe );
    cqe = nullptr;
  }

  // Resume any newly ready coroutines
  resume_ready_coroutines();
}

OperationContext* IoUringRuntime::submit_sqe( std::function<void( io_uring_sqe* )> prepare_sqe )
{
  // Get a submission queue entry
  io_uring_sqe* sqe = io_uring_get_sqe( &ring_ );
  if ( !sqe ) {
    // If queue is full, submit and try again
    submit();
    sqe = io_uring_get_sqe( &ring_ );
    if ( !sqe ) {
      throw std::runtime_error( "Failed to get SQE even after submit" );
    }
  }

  // Create operation context
  uint64_t operation_id = next_operation_id_++;
  auto ctx = std::make_unique<OperationContext>( nullptr );
  auto* ctx_ptr = ctx.get();

  // Prepare the SQE
  prepare_sqe( sqe );

  // Set user_data to track this operation
  io_uring_sqe_set_data64( sqe, operation_id );

  // Store the context
  pending_operations_[operation_id] = std::move( ctx );

  return ctx_ptr;
}

void IoUringRuntime::schedule( std::coroutine_handle<> handle )
{
  if ( handle && !handle.done() ) {
    ready_queue_.push( handle );
  }
}

bool IoUringRuntime::has_pending_work() const noexcept
{
  return !ready_queue_.empty() || !pending_operations_.empty();
}

IoUringRuntime* IoUringRuntime::current() noexcept
{
  return current_runtime_;
}

void IoUringRuntime::process_completions()
{
  io_uring_cqe* cqe = nullptr;

  // Wait for at least one completion
  int ret = io_uring_wait_cqe( &ring_, &cqe );
  if ( ret < 0 ) {
    if ( ret == -EINTR ) {
      return; // Interrupted, try again
    }
    throw std::system_error( -ret, std::system_category(), "io_uring_wait_cqe failed" );
  }

  // Process this completion
  if ( cqe ) {
    uint64_t operation_id = io_uring_cqe_get_data64( cqe );

    if ( operation_id != 0 ) {
      auto it = pending_operations_.find( operation_id );
      if ( it != pending_operations_.end() ) {
        auto& ctx = it->second;
        ctx->result = cqe->res;
        ctx->completed = true;

        // Schedule coroutine to resume
        if ( ctx->coroutine ) {
          ready_queue_.push( ctx->coroutine );
        }

        pending_operations_.erase( it );
      }
    }

    io_uring_cqe_seen( &ring_, cqe );
  }

  // Process any additional completions that are ready
  while ( io_uring_peek_cqe( &ring_, &cqe ) == 0 && cqe != nullptr ) {
    uint64_t operation_id = io_uring_cqe_get_data64( cqe );

    if ( operation_id != 0 ) {
      auto it = pending_operations_.find( operation_id );
      if ( it != pending_operations_.end() ) {
        auto& ctx = it->second;
        ctx->result = cqe->res;
        ctx->completed = true;

        // Schedule coroutine to resume
        if ( ctx->coroutine ) {
          ready_queue_.push( ctx->coroutine );
        }

        pending_operations_.erase( it );
      }
    }

    io_uring_cqe_seen( &ring_, cqe );
    cqe = nullptr;
  }
}

void IoUringRuntime::submit()
{
  int ret = io_uring_submit( &ring_ );
  if ( ret < 0 ) {
    throw std::system_error( -ret, std::system_category(), "io_uring_submit failed" );
  }
}

void IoUringRuntime::resume_ready_coroutines()
{
  while ( !ready_queue_.empty() ) {
    auto handle = ready_queue_.front();
    ready_queue_.pop();

    if ( handle && !handle.done() ) {
      handle.resume();
    }
  }
}

} // namespace async
