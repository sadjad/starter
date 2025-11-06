#pragma once

// Main header that includes all async/coroutine functionality

#include "task.hh"
#include "io_uring_runtime.hh"
#include "awaitable_ops.hh"
#include "utils.hh"

// This header provides a complete coroutine-based async I/O API using io_uring
//
// Basic usage:
//
//   Task<void> my_async_function() {
//     char buffer[1024];
//     ssize_t n = co_await async_read(fd, buffer, sizeof(buffer));
//     co_await async_write(fd, buffer, n);
//   }
//
//   int main() {
//     IoUringRuntime runtime;
//     runtime.spawn(my_async_function());
//     runtime.run();
//   }
