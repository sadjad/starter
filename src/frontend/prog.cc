#include "util/async_file.hh"
#include "util/eventloop.hh"
#include "util/temp_file.hh"

#include <array>
#include <iostream>
#include <unistd.h>

using namespace std;

static Task<void> example( EventLoop& loop )
{
  TempFile file( "async_sample" );
  FileDescriptor& fd = file.fd();

  co_await async_write_all( loop, fd, "hello from async\n" );

  ::lseek( fd.fd_num(), 0, SEEK_SET );

  array<char, 64> buffer;
  simple_string_span span { buffer.data(), buffer.size() };
  const size_t bytes = co_await async_read( loop, fd, span );

  cout.write( span.data(), bytes );
  cout.flush();
  co_return;
}

int main()
{
  EventLoop loop;
  sync_wait( loop, example( loop ) );
  return 0;
}
