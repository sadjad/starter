#include "async/async.hh"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace async;

// Create a non-blocking TCP listening socket
int create_listening_socket( uint16_t port )
{
  int fd = socket( AF_INET, SOCK_STREAM, 0 );
  if ( fd < 0 ) {
    throw std::system_error( errno, std::system_category(), "socket() failed" );
  }

  // Set SO_REUSEADDR
  int optval = 1;
  if ( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) ) < 0 ) {
    close( fd );
    throw std::system_error( errno, std::system_category(), "setsockopt(SO_REUSEADDR) failed" );
  }

  // Make non-blocking
  int flags = fcntl( fd, F_GETFL, 0 );
  if ( flags < 0 || fcntl( fd, F_SETFL, flags | O_NONBLOCK ) < 0 ) {
    close( fd );
    throw std::system_error( errno, std::system_category(), "fcntl(O_NONBLOCK) failed" );
  }

  // Bind
  sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons( port );

  if ( bind( fd, reinterpret_cast<sockaddr*>( &addr ), sizeof( addr ) ) < 0 ) {
    close( fd );
    throw std::system_error( errno, std::system_category(), "bind() failed" );
  }

  // Listen
  if ( listen( fd, 128 ) < 0 ) {
    close( fd );
    throw std::system_error( errno, std::system_category(), "listen() failed" );
  }

  return fd;
}

// Handle a single client connection
Task<void> handle_client( int client_fd )
{
  std::cout << "Client connected: fd=" << client_fd << std::endl;

  char buffer[4096];

  try {
    while ( true ) {
      // Read data from client
      ssize_t bytes_read = co_await async_read( client_fd, buffer, sizeof( buffer ) );

      if ( bytes_read == 0 ) {
        std::cout << "Client disconnected: fd=" << client_fd << std::endl;
        break;
      }

      std::cout << "Received " << bytes_read << " bytes from fd=" << client_fd << std::endl;

      // Echo data back to client
      ssize_t bytes_written = 0;
      while ( bytes_written < bytes_read ) {
        ssize_t n = co_await async_write(
          client_fd, buffer + bytes_written, bytes_read - bytes_written );
        bytes_written += n;
      }

      std::cout << "Echoed " << bytes_written << " bytes to fd=" << client_fd << std::endl;
    }
  } catch ( const std::exception& e ) {
    std::cerr << "Error handling client fd=" << client_fd << ": " << e.what() << std::endl;
  }

  // Close the client socket
  co_await async_close( client_fd );
  std::cout << "Closed connection: fd=" << client_fd << std::endl;
}

// Main server loop
Task<void> server( uint16_t port )
{
  int listen_fd = create_listening_socket( port );
  std::cout << "Echo server listening on port " << port << std::endl;

  try {
    while ( true ) {
      // Accept a new connection
      sockaddr_in client_addr {};
      socklen_t client_addr_len = sizeof( client_addr );

      int client_fd
        = co_await async_accept( listen_fd, reinterpret_cast<sockaddr*>( &client_addr ), &client_addr_len );

      // Get client address string
      char addr_str[INET_ADDRSTRLEN];
      inet_ntop( AF_INET, &client_addr.sin_addr, addr_str, sizeof( addr_str ) );

      std::cout << "Accepted connection from " << addr_str << ":" << ntohs( client_addr.sin_port )
                << " (fd=" << client_fd << ")" << std::endl;

      // Spawn a coroutine to handle this client
      spawn( handle_client( client_fd ) );
    }
  } catch ( const std::exception& e ) {
    std::cerr << "Server error: " << e.what() << std::endl;
  }

  close( listen_fd );
}

int main()
{
  try {
    IoUringRuntime runtime( 256 );
    runtime.spawn( server( 8080 ) );
    runtime.run();
  } catch ( const std::exception& e ) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
