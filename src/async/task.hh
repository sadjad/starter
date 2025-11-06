#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace async {

// Forward declaration
template<typename T>
class Task;

namespace detail {

// Base promise type for common functionality
template<typename T>
class TaskPromiseBase
{
public:
  std::suspend_always initial_suspend() noexcept { return {}; }

  struct FinalAwaiter
  {
    bool await_ready() noexcept { return false; }

    template<typename Promise>
    std::coroutine_handle<> await_suspend( std::coroutine_handle<Promise> h ) noexcept
    {
      auto& promise = h.promise();
      if ( promise.continuation_ ) {
        return promise.continuation_;
      }
      return std::noop_coroutine();
    }

    void await_resume() noexcept {}
  };

  FinalAwaiter final_suspend() noexcept { return {}; }

  void unhandled_exception() noexcept { exception_ = std::current_exception(); }

  void set_continuation( std::coroutine_handle<> continuation ) noexcept
  {
    continuation_ = continuation;
  }

protected:
  std::coroutine_handle<> continuation_;
  std::exception_ptr exception_;
};

} // namespace detail

// Task promise type for T (non-void)
template<typename T>
class TaskPromise : public detail::TaskPromiseBase<T>
{
public:
  Task<T> get_return_object() noexcept;

  void return_value( T value ) noexcept { result_.emplace( std::move( value ) ); }

  T result()
  {
    if ( this->exception_ ) {
      std::rethrow_exception( this->exception_ );
    }
    return std::move( *result_ );
  }

private:
  std::optional<T> result_;
};

// Task promise type for void
template<>
class TaskPromise<void> : public detail::TaskPromiseBase<void>
{
public:
  Task<void> get_return_object() noexcept;

  void return_void() noexcept {}

  void result()
  {
    if ( this->exception_ ) {
      std::rethrow_exception( this->exception_ );
    }
  }
};

// Task<T> - the coroutine return type
template<typename T>
class [[nodiscard]] Task
{
public:
  using promise_type = TaskPromise<T>;
  using handle_type = std::coroutine_handle<promise_type>;

  explicit Task( handle_type handle ) noexcept
    : handle_( handle )
  {}

  Task( Task&& other ) noexcept
    : handle_( std::exchange( other.handle_, nullptr ) )
  {}

  Task& operator=( Task&& other ) noexcept
  {
    if ( this != &other ) {
      if ( handle_ ) {
        handle_.destroy();
      }
      handle_ = std::exchange( other.handle_, nullptr );
    }
    return *this;
  }

  ~Task()
  {
    if ( handle_ ) {
      handle_.destroy();
    }
  }

  Task( const Task& ) = delete;
  Task& operator=( const Task& ) = delete;

  // Awaitable interface - allows co_await task
  bool await_ready() const noexcept { return false; }

  std::coroutine_handle<> await_suspend( std::coroutine_handle<> continuation ) noexcept
  {
    handle_.promise().set_continuation( continuation );
    return handle_;
  }

  T await_resume()
  {
    if constexpr ( std::is_void_v<T> ) {
      handle_.promise().result();
    } else {
      return handle_.promise().result();
    }
  }

  // Start the coroutine without waiting (fire and forget)
  void detach()
  {
    if ( handle_ ) {
      handle_.resume();
      handle_ = nullptr;
    }
  }

  // Get the coroutine handle (for runtime management)
  handle_type handle() const noexcept { return handle_; }

  // Check if the task has completed
  bool done() const noexcept { return handle_ && handle_.done(); }

private:
  handle_type handle_;
};

// Implementation of get_return_object
template<typename T>
inline Task<T> TaskPromise<T>::get_return_object() noexcept
{
  return Task<T> { std::coroutine_handle<TaskPromise<T>>::from_promise( *this ) };
}

inline Task<void> TaskPromise<void>::get_return_object() noexcept
{
  return Task<void> { std::coroutine_handle<TaskPromise<void>>::from_promise( *this ) };
}

} // namespace async
