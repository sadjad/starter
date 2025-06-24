#pragma once

#include "eventloop.hh"
#include "file_descriptor.hh"
#include "task.hh"
#include <optional>
#include <type_traits>
#include <variant>

/** Awaitable that suspends until a file descriptor is readable. */
class WaitReadable
{
  std::optional<EventLoop::RuleHandle> rule_ {};
  std::coroutine_handle<> handle_ {};
  EventLoop* loop_;
  FileDescriptor* fd_;

public:
  WaitReadable( EventLoop& loop, FileDescriptor& fd )
    : loop_( &loop )
    , fd_( &fd )
  {
  }

  WaitReadable( const WaitReadable& ) = delete;
  WaitReadable& operator=( const WaitReadable& ) = delete;

  bool await_ready() const noexcept { return false; }

  void await_suspend( std::coroutine_handle<> h )
  {
    handle_ = h;
    rule_ = loop_->add_rule(
      "await read",
      Direction::In,
      *fd_,
      [this] {
        rule_->cancel();
        handle_.resume();
      },
      [] { return true; } );
    loop_ = nullptr;
    fd_ = nullptr;
  }

  void await_resume() const noexcept {}
};

/** Awaitable that suspends until a file descriptor is writeable. */
class WaitWriteable
{
  std::optional<EventLoop::RuleHandle> rule_ {};
  std::coroutine_handle<> handle_ {};
  EventLoop* loop_;
  FileDescriptor* fd_;

public:
  WaitWriteable( EventLoop& loop, FileDescriptor& fd )
    : loop_( &loop )
    , fd_( &fd )
  {
  }

  WaitWriteable( const WaitWriteable& ) = delete;
  WaitWriteable& operator=( const WaitWriteable& ) = delete;

  bool await_ready() const noexcept { return false; }

  void await_suspend( std::coroutine_handle<> h )
  {
    handle_ = h;
    rule_ = loop_->add_rule(
      "await write",
      Direction::Out,
      *fd_,
      [this] {
        rule_->cancel();
        handle_.resume();
      },
      [] { return true; } );
    loop_ = nullptr;
    fd_ = nullptr;
  }

  void await_resume() const noexcept {}
};

inline Task<size_t> async_read( EventLoop& loop, FileDescriptor& fd, simple_string_span buffer )
{
  while ( true ) {
    const size_t bytes = fd.read( buffer );
    if ( bytes > 0 || fd.eof() ) {
      co_return bytes;
    }
    co_await WaitReadable { loop, fd };
  }
}

inline Task<size_t> async_write( EventLoop& loop, FileDescriptor& fd, std::string_view buffer )
{
  while ( true ) {
    const size_t bytes = fd.write( buffer );
    if ( bytes > 0 || buffer.empty() ) {
      co_return bytes;
    }
    co_await WaitWriteable { loop, fd };
  }
}

inline Task<void> async_write_all( EventLoop& loop, FileDescriptor& fd, std::string_view buffer )
{
  while ( not buffer.empty() ) {
    const size_t written = co_await async_write( loop, fd, buffer );
    buffer.remove_prefix( written );
  }
  co_return;
}

/** Run the event loop until the given task finishes and return its result. */
template<typename T>
T sync_wait( EventLoop& loop, Task<T> t )
{
  using result_t = std::conditional_t<std::is_void_v<T>, std::monostate, std::optional<T>>;
  result_t result {};
  std::exception_ptr ep;
  bool done = false;

  auto wrapper = [&]() -> Task<void> {
    try {
      if constexpr ( std::is_void_v<T> ) {
        co_await t;
      } else {
        result = co_await t;
      }
    } catch ( ... ) {
      ep = std::current_exception();
    }
    done = true;
    co_return;
  }();

  wrapper.start();

  while ( not done ) {
    if ( loop.wait_next_event( -1 ) == EventLoop::Result::Exit ) {
      break;
    }
  }

  if ( ep ) {
    std::rethrow_exception( ep );
  }

  if constexpr ( std::is_void_v<T> ) {
    return;
  } else {
    return *result;
  }
}
