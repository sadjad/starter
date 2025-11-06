#pragma once

#include <vector>
#include <tuple>
#include <utility>

#include "task.hh"
#include "awaitable_ops.hh"

namespace async {

// Sleep for a specified duration
inline Task<void> sleep_for( uint64_t nanoseconds )
{
  co_await async_timeout( nanoseconds );
}

// Spawn a task and detach it (fire-and-forget)
template<typename T>
void spawn( Task<T>&& task )
{
  current_runtime().spawn( std::move( task ) );
}

// when_all - wait for all tasks to complete
template<typename... Tasks>
Task<std::tuple<typename Tasks::promise_type::result_type...>> when_all( Tasks&&... tasks )
{
  // This is a simplified version - a full implementation would handle
  // concurrent execution better. For now, we'll await them sequentially.
  co_return std::make_tuple( co_await std::forward<Tasks>( tasks )... );
}

// Simpler when_all for void tasks
inline Task<void> when_all_void( std::vector<Task<void>> tasks )
{
  for ( auto& task : tasks ) {
    co_await task;
  }
}

// Helper to create a vector of tasks
template<typename Func, typename... Args>
std::vector<Task<void>> make_tasks( size_t count, Func func, Args&&... args )
{
  std::vector<Task<void>> tasks;
  tasks.reserve( count );
  for ( size_t i = 0; i < count; ++i ) {
    tasks.push_back( func( std::forward<Args>( args )... ) );
  }
  return tasks;
}

} // namespace async
