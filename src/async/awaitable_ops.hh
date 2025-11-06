#pragma once

#include <coroutine>
#include <liburing.h>
#include <system_error>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>

#include "io_uring_runtime.hh"

namespace async {

// Base awaitable operation
template<typename Derived>
class AwaitableBase
{
public:
  bool await_ready() const noexcept { return false; }

protected:
  OperationContext* ctx_ { nullptr };
};

// AsyncRead - read from a file descriptor
class AsyncRead : public AwaitableBase<AsyncRead>
{
public:
  AsyncRead( int fd, void* buf, size_t len, uint64_t offset = -1 )
    : fd_( fd )
    , buf_( buf )
    , len_( len )
    , offset_( offset )
  {}

  void await_suspend( std::coroutine_handle<> handle )
  {
    auto& runtime = current_runtime();

    ctx_ = runtime.submit_sqe( [this]( io_uring_sqe* sqe ) {
      if ( offset_ == static_cast<uint64_t>( -1 ) ) {
        io_uring_prep_read( sqe, fd_, buf_, len_, 0 );
      } else {
        io_uring_prep_read( sqe, fd_, buf_, len_, offset_ );
      }
    } );

    ctx_->coroutine = handle;
  }

  ssize_t await_resume()
  {
    int32_t result = ctx_->result;
    if ( result < 0 ) {
      throw std::system_error( -result, std::system_category(), "async_read failed" );
    }
    return result;
  }

private:
  int fd_;
  void* buf_;
  size_t len_;
  uint64_t offset_;
};

// AsyncWrite - write to a file descriptor
class AsyncWrite : public AwaitableBase<AsyncWrite>
{
public:
  AsyncWrite( int fd, const void* buf, size_t len, uint64_t offset = -1 )
    : fd_( fd )
    , buf_( buf )
    , len_( len )
    , offset_( offset )
  {}

  void await_suspend( std::coroutine_handle<> handle )
  {
    auto& runtime = current_runtime();

    ctx_ = runtime.submit_sqe( [this]( io_uring_sqe* sqe ) {
      if ( offset_ == static_cast<uint64_t>( -1 ) ) {
        io_uring_prep_write( sqe, fd_, buf_, len_, 0 );
      } else {
        io_uring_prep_write( sqe, fd_, buf_, len_, offset_ );
      }
    } );

    ctx_->coroutine = handle;
  }

  ssize_t await_resume()
  {
    int32_t result = ctx_->result;
    if ( result < 0 ) {
      throw std::system_error( -result, std::system_category(), "async_write failed" );
    }
    return result;
  }

private:
  int fd_;
  const void* buf_;
  size_t len_;
  uint64_t offset_;
};

// AsyncAccept - accept a connection on a listening socket
class AsyncAccept : public AwaitableBase<AsyncAccept>
{
public:
  AsyncAccept( int fd, sockaddr* addr, socklen_t* addrlen )
    : fd_( fd )
    , addr_( addr )
    , addrlen_( addrlen )
  {}

  void await_suspend( std::coroutine_handle<> handle )
  {
    auto& runtime = current_runtime();

    ctx_ = runtime.submit_sqe( [this]( io_uring_sqe* sqe ) {
      io_uring_prep_accept( sqe, fd_, addr_, addrlen_, 0 );
    } );

    ctx_->coroutine = handle;
  }

  int await_resume()
  {
    int32_t result = ctx_->result;
    if ( result < 0 ) {
      throw std::system_error( -result, std::system_category(), "async_accept failed" );
    }
    return result;
  }

private:
  int fd_;
  sockaddr* addr_;
  socklen_t* addrlen_;
};

// AsyncConnect - connect to a remote address
class AsyncConnect : public AwaitableBase<AsyncConnect>
{
public:
  AsyncConnect( int fd, const sockaddr* addr, socklen_t addrlen )
    : fd_( fd )
    , addr_( addr )
    , addrlen_( addrlen )
  {}

  void await_suspend( std::coroutine_handle<> handle )
  {
    auto& runtime = current_runtime();

    ctx_ = runtime.submit_sqe( [this]( io_uring_sqe* sqe ) {
      io_uring_prep_connect( sqe, fd_, addr_, addrlen_ );
    } );

    ctx_->coroutine = handle;
  }

  void await_resume()
  {
    int32_t result = ctx_->result;
    if ( result < 0 ) {
      throw std::system_error( -result, std::system_category(), "async_connect failed" );
    }
  }

private:
  int fd_;
  const sockaddr* addr_;
  socklen_t addrlen_;
};

// AsyncClose - close a file descriptor
class AsyncClose : public AwaitableBase<AsyncClose>
{
public:
  explicit AsyncClose( int fd )
    : fd_( fd )
  {}

  void await_suspend( std::coroutine_handle<> handle )
  {
    auto& runtime = current_runtime();

    ctx_ = runtime.submit_sqe( [this]( io_uring_sqe* sqe ) { io_uring_prep_close( sqe, fd_ ); } );

    ctx_->coroutine = handle;
  }

  void await_resume()
  {
    int32_t result = ctx_->result;
    if ( result < 0 ) {
      throw std::system_error( -result, std::system_category(), "async_close failed" );
    }
  }

private:
  int fd_;
};

// AsyncTimeout - wait for a specified duration
class AsyncTimeout : public AwaitableBase<AsyncTimeout>
{
public:
  explicit AsyncTimeout( uint64_t nanoseconds )
    : nanoseconds_( nanoseconds )
  {
    ts_.tv_sec = nanoseconds / 1'000'000'000;
    ts_.tv_nsec = nanoseconds % 1'000'000'000;
  }

  void await_suspend( std::coroutine_handle<> handle )
  {
    auto& runtime = current_runtime();

    ctx_ = runtime.submit_sqe( [this]( io_uring_sqe* sqe ) {
      io_uring_prep_timeout( sqe, &ts_, 0, 0 );
    } );

    ctx_->coroutine = handle;
  }

  void await_resume()
  {
    // Timeout returns -ETIME when it expires, which is expected
    int32_t result = ctx_->result;
    if ( result < 0 && result != -ETIME ) {
      throw std::system_error( -result, std::system_category(), "async_timeout failed" );
    }
  }

private:
  uint64_t nanoseconds_;
  __kernel_timespec ts_;
};

// Convenience functions
inline AsyncRead async_read( int fd, void* buf, size_t len )
{
  return AsyncRead( fd, buf, len );
}

inline AsyncRead async_read( int fd, void* buf, size_t len, uint64_t offset )
{
  return AsyncRead( fd, buf, len, offset );
}

inline AsyncWrite async_write( int fd, const void* buf, size_t len )
{
  return AsyncWrite( fd, buf, len );
}

inline AsyncWrite async_write( int fd, const void* buf, size_t len, uint64_t offset )
{
  return AsyncWrite( fd, buf, len, offset );
}

inline AsyncAccept async_accept( int fd, sockaddr* addr = nullptr, socklen_t* addrlen = nullptr )
{
  return AsyncAccept( fd, addr, addrlen );
}

inline AsyncConnect async_connect( int fd, const sockaddr* addr, socklen_t addrlen )
{
  return AsyncConnect( fd, addr, addrlen );
}

inline AsyncClose async_close( int fd )
{
  return AsyncClose( fd );
}

inline AsyncTimeout async_timeout( uint64_t nanoseconds )
{
  return AsyncTimeout( nanoseconds );
}

// Helper for common time units
namespace time {

constexpr uint64_t nanoseconds( uint64_t ns )
{
  return ns;
}

constexpr uint64_t microseconds( uint64_t us )
{
  return us * 1'000;
}

constexpr uint64_t milliseconds( uint64_t ms )
{
  return ms * 1'000'000;
}

constexpr uint64_t seconds( uint64_t s )
{
  return s * 1'000'000'000;
}

} // namespace time

} // namespace async
