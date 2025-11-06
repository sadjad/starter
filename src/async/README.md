# Async I/O with io_uring and C++ Coroutines

This directory contains a modern C++20 coroutine-based async I/O library built on top of Linux's io_uring.

## Overview

This library provides a clean, type-safe async/await interface for I/O operations using C++20 coroutines and io_uring. It runs completely independently of the existing epoll-based EventLoop, allowing gradual migration or parallel usage.

## Features

- **C++20 Coroutines**: Clean async/await syntax
- **io_uring Backend**: High-performance, zero-copy I/O
- **Type-Safe**: Compiler-checked async operations
- **Zero Dependencies**: No impact on existing code
- **Composable**: Easy to build complex async workflows

## Architecture

### Core Components

1. **Task<T>** (`task.hh`)
   - Coroutine return type with promise_type
   - Supports both `co_await` (composition) and `.detach()` (fire-and-forget)
   - Handles both value-returning and void coroutines

2. **IoUringRuntime** (`io_uring_runtime.hh/cc`)
   - Manages io_uring instance (submission queue, completion queue)
   - Event loop that processes completions and resumes coroutines
   - Thread-local runtime context

3. **Awaitable Operations** (`awaitable_ops.hh`)
   - `async_read()` - Read from file descriptor
   - `async_write()` - Write to file descriptor
   - `async_accept()` - Accept TCP connection
   - `async_connect()` - Connect to remote address
   - `async_close()` - Close file descriptor
   - `async_timeout()` - Wait for duration

4. **Utilities** (`utils.hh`)
   - `sleep_for()` - Convenient sleep function
   - `spawn()` - Launch task without waiting
   - `when_all()` - Wait for multiple tasks

## Requirements

### System Requirements

- **Linux kernel 5.1+** (io_uring support)
- **liburing** development library
- **C++23 compiler** (GCC 10+, Clang 10+)

### Installing liburing

**Ubuntu/Debian:**
```bash
sudo apt-get install liburing-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install liburing-devel
```

**Arch Linux:**
```bash
sudo pacman -S liburing
```

**From source:**
```bash
git clone https://github.com/axboe/liburing.git
cd liburing
./configure
make
sudo make install
```

## Usage

### Basic Example

```cpp
#include "async/async.hh"

using namespace async;

Task<void> echo_data(int fd) {
  char buffer[1024];

  // Read data (suspends until data available)
  ssize_t n = co_await async_read(fd, buffer, sizeof(buffer));

  // Write data back (suspends until write completes)
  co_await async_write(fd, buffer, n);
}

int main() {
  IoUringRuntime runtime;
  runtime.spawn(echo_data(some_fd));
  runtime.run();  // Blocks until all tasks complete
}
```

### Echo Server Example

```cpp
Task<void> handle_client(int client_fd) {
  char buffer[4096];

  while (true) {
    ssize_t n = co_await async_read(client_fd, buffer, sizeof(buffer));
    if (n == 0) break;  // EOF

    co_await async_write(client_fd, buffer, n);
  }

  co_await async_close(client_fd);
}

Task<void> server(int listen_fd) {
  while (true) {
    int client_fd = co_await async_accept(listen_fd, nullptr, nullptr);
    spawn(handle_client(client_fd));  // Handle concurrently
  }
}

int main() {
  int listen_fd = create_listening_socket(8080);

  IoUringRuntime runtime(256);  // Queue depth
  runtime.spawn(server(listen_fd));
  runtime.run();
}
```

### HTTP Client Example

```cpp
Task<std::string> fetch_url(const std::string& host, uint16_t port) {
  int fd = create_socket();
  sockaddr_in addr = resolve(host, port);

  // Connect (suspends until connected)
  co_await async_connect(fd, (sockaddr*)&addr, sizeof(addr));

  // Send request
  std::string request = "GET / HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
  co_await async_write(fd, request.data(), request.size());

  // Read response
  std::string response;
  char buffer[4096];
  while (true) {
    ssize_t n = co_await async_read(fd, buffer, sizeof(buffer));
    if (n == 0) break;
    response.append(buffer, n);
  }

  co_await async_close(fd);
  co_return response;
}

Task<void> main_task() {
  // Fetch multiple URLs concurrently
  auto task1 = fetch_url("example.com", 80);
  auto task2 = fetch_url("example.org", 80);

  std::string response1 = co_await task1;
  std::string response2 = co_await task2;

  std::cout << "Fetched " << response1.size() << " and "
            << response2.size() << " bytes" << std::endl;
}
```

### Timeouts and Delays

```cpp
Task<void> delayed_task() {
  std::cout << "Starting..." << std::endl;

  co_await sleep_for(time::seconds(1));

  std::cout << "After 1 second" << std::endl;

  co_await sleep_for(time::milliseconds(500));

  std::cout << "After 1.5 seconds" << std::endl;
}
```

## How It Works

### 1. Coroutine Suspension

When you `co_await async_read(fd, buf, len)`:

```cpp
AsyncRead::await_suspend(handle) {
  1. Get current IoUringRuntime
  2. Submit read operation to io_uring submission queue
  3. Store coroutine handle with operation
  4. Suspend coroutine (returns control to runtime)
}
```

### 2. Event Loop Processing

```cpp
IoUringRuntime::run() {
  while (has_pending_work()) {
    1. Resume any ready coroutines
    2. Submit pending operations (io_uring_submit)
    3. Wait for completions (io_uring_wait_cqe)
    4. Process completions:
       - Store result in OperationContext
       - Schedule coroutine to resume
    5. Loop back
  }
}
```

### 3. Coroutine Resumption

```cpp
AsyncRead::await_resume() {
  1. Get result from OperationContext
  2. Check for errors (throw if negative)
  3. Return bytes read
  4. Coroutine continues from co_await point
}
```

## Performance Benefits

### io_uring Advantages

1. **Batched Operations**: Submit multiple operations at once
2. **Zero-Copy**: Direct kernel/userspace memory sharing (with IORING_SETUP_IOPOLL)
3. **Reduced Syscalls**: Submit and wait in single syscall
4. **Async Everything**: Truly async accept, connect, read, write, close

### Compared to epoll

| Feature | epoll | io_uring |
|---------|-------|----------|
| Syscalls per operation | 2-3 | 0-1 (batched) |
| Copy operations | Multiple | Zero-copy capable |
| Async accept | No | Yes |
| Async connect | Partial | Yes |
| File I/O | Blocking | Truly async |

## Integration with Existing Code

This library is **completely independent** of the existing epoll-based EventLoop:

```cpp
// Old code - still works
EventLoop loop;
TCPSocket socket;
loop.add_rule(socket.fd(), /* ... */);

// New code - runs separately
IoUringRuntime runtime;
runtime.spawn(async_handler(socket.fd_num()));
runtime.run();
```

You can:
- Run old code with EventLoop
- Run new code with IoUringRuntime
- Gradually migrate components
- Keep both in production

## File Structure

```
src/async/
├── async.hh               # Main include file
├── task.hh                # Task<T> coroutine type
├── io_uring_runtime.hh    # Runtime header
├── io_uring_runtime.cc    # Runtime implementation
├── awaitable_ops.hh       # Async operations
├── utils.hh               # Utility functions
└── README.md              # This file

src/examples/
├── echo_server_async.cc   # Echo server example
└── http_client_async.cc   # HTTP client example
```

## Building

The CMakeLists.txt has been updated to include the async library:

```bash
mkdir build
cd build
cmake ..
make echo_server_async
make http_client_async
```

Run the examples:

```bash
# Terminal 1: Run echo server
./echo_server_async

# Terminal 2: Test with netcat
echo "Hello, io_uring!" | nc localhost 8080

# Run HTTP client
./http_client_async
```

## Error Handling

All operations throw `std::system_error` on failure:

```cpp
Task<void> safe_operation(int fd) {
  try {
    co_await async_read(fd, buffer, size);
  } catch (const std::system_error& e) {
    std::cerr << "I/O error: " << e.what() << std::endl;
    // Handle error
  }
}
```

## Thread Safety

- Each thread needs its own `IoUringRuntime`
- `IoUringRuntime` is **not thread-safe** (by design)
- `Task<T>` can be moved between threads before starting
- Once started, coroutines run on the runtime's thread

## Limitations

1. **Linux-only**: io_uring is Linux-specific
2. **Kernel 5.1+**: Older kernels not supported
3. **Single-threaded**: Each runtime is single-threaded (spawn multiple runtimes for multi-threading)
4. **No cancellation**: Operation cancellation not yet implemented

## Future Enhancements

- [ ] Operation cancellation support
- [ ] Multi-shot operations (IORING_SETUP_SQPOLL)
- [ ] File I/O with buffer registration
- [ ] UDP socket operations (sendmsg, recvmsg)
- [ ] Zero-copy send/receive (IORING_OP_SEND_ZC)
- [ ] when_any() and select() utilities
- [ ] Timeout support for individual operations
- [ ] Multi-threaded runtime (thread pool)

## References

- [io_uring Documentation](https://kernel.dk/io_uring.pdf)
- [liburing Library](https://github.com/axboe/liburing)
- [C++20 Coroutines](https://en.cppreference.com/w/cpp/language/coroutines)
- [Efficient IO with io_uring](https://kernel.dk/io_uring.pdf)

## License

Same as the parent project.
