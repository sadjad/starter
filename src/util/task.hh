#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

/** A simple coroutine task similar to Python's awaitables. */

template<typename T>
class Task
{
public:
  struct promise_type
  {
    std::optional<T> value_ {};
    std::exception_ptr exception_ {};
    std::coroutine_handle<> continuation_ {};

    Task get_return_object() { return Task { std::coroutine_handle<promise_type>::from_promise( *this ) }; }

    std::suspend_always initial_suspend() const noexcept { return {}; }

    struct FinalAwaiter
    {
      bool await_ready() const noexcept { return false; }
      void await_suspend( std::coroutine_handle<promise_type> h ) const noexcept
      {
        if ( h.promise().continuation_ ) {
          h.promise().continuation_.resume();
        }
      }
      void await_resume() const noexcept {}
    };

    auto final_suspend() const noexcept { return FinalAwaiter {}; }

    template<typename U>
    void return_value( U&& v )
    {
      value_ = std::forward<U>( v );
    }

    void unhandled_exception() { exception_ = std::current_exception(); }
  };

  using handle_type = std::coroutine_handle<promise_type>;

  explicit Task( handle_type h )
    : handle_( h )
  {
  }

  Task( Task&& other ) noexcept
    : handle_( other.handle_ )
  {
    other.handle_ = nullptr;
  }

  Task( const Task& ) = delete;
  Task& operator=( const Task& ) = delete;

  ~Task()
  {
    if ( handle_ ) {
      handle_.destroy();
    }
  }

  void start()
  {
    if ( handle_ ) {
      handle_.resume();
    }
  }

  bool done() const { return not handle_ || handle_.done(); }

  bool await_ready() const noexcept { return done(); }

  void await_suspend( std::coroutine_handle<> h ) noexcept
  {
    handle_.promise().continuation_ = h;
    handle_.resume();
  }

  T await_resume()
  {
    if ( handle_.promise().exception_ ) {
      std::rethrow_exception( handle_.promise().exception_ );
    }
    return std::move( *handle_.promise().value_ );
  }

private:
  handle_type handle_ { nullptr };
};

// specialization for void
template<>
class Task<void>
{
public:
  struct promise_type
  {
    std::exception_ptr exception_ {};
    std::coroutine_handle<> continuation_ {};

    Task get_return_object() { return Task { std::coroutine_handle<promise_type>::from_promise( *this ) }; }

    std::suspend_always initial_suspend() const noexcept { return {}; }

    struct FinalAwaiter
    {
      bool await_ready() const noexcept { return false; }
      void await_suspend( std::coroutine_handle<promise_type> h ) const noexcept
      {
        if ( h.promise().continuation_ ) {
          h.promise().continuation_.resume();
        }
      }
      void await_resume() const noexcept {}
    };

    auto final_suspend() const noexcept { return FinalAwaiter {}; }

    void return_void() {}

    void unhandled_exception() { exception_ = std::current_exception(); }
  };

  using handle_type = std::coroutine_handle<promise_type>;

  explicit Task( handle_type h )
    : handle_( h )
  {
  }

  Task( Task&& other ) noexcept
    : handle_( other.handle_ )
  {
    other.handle_ = nullptr;
  }

  Task( const Task& ) = delete;
  Task& operator=( const Task& ) = delete;

  ~Task()
  {
    if ( handle_ ) {
      handle_.destroy();
    }
  }

  void start()
  {
    if ( handle_ ) {
      handle_.resume();
    }
  }

  bool done() const { return not handle_ || handle_.done(); }

  bool await_ready() const noexcept { return done(); }

  void await_suspend( std::coroutine_handle<> h ) noexcept
  {
    handle_.promise().continuation_ = h;
    handle_.resume();
  }

  void await_resume()
  {
    if ( handle_.promise().exception_ ) {
      std::rethrow_exception( handle_.promise().exception_ );
    }
  }

private:
  handle_type handle_ { nullptr };
};
