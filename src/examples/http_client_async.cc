#include "async/async.hh"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

using namespace async;

// Create a non-blocking TCP socket
int create_socket()
{
  int fd = socket( AF_INET, SOCK_STREAM, 0 );
  if ( fd < 0 ) {
    throw std::system_error( errno, std::system_category(), "socket() failed" );
  }

  // Make non-blocking
  int flags = fcntl( fd, F_GETFL, 0 );
  if ( flags < 0 || fcntl( fd, F_SETFL, flags | O_NONBLOCK ) < 0 ) {
    close( fd );
    throw std::system_error( errno, std::system_category(), "fcntl(O_NONBLOCK) failed" );
  }

  return fd;
}

// Resolve hostname to IP address
std::string resolve_hostname( const std::string& hostname )
{
  addrinfo hints {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* result = nullptr;
  int ret = getaddrinfo( hostname.c_str(), nullptr, &hints, &result );
  if ( ret != 0 ) {
    throw std::runtime_error( std::string( "getaddrinfo failed: " ) + gai_strerror( ret ) );
  }

  char addr_str[INET_ADDRSTRLEN];
  auto* addr_in = reinterpret_cast<sockaddr_in*>( result->ai_addr );
  inet_ntop( AF_INET, &addr_in->sin_addr, addr_str, sizeof( addr_str ) );

  std::string ip_address( addr_str );
  freeaddrinfo( result );

  return ip_address;
}

// Fetch a URL using HTTP/1.1
Task<std::string> fetch_url( const std::string& hostname, uint16_t port, const std::string& path )
{
  std::cout << "Resolving " << hostname << "..." << std::endl;
  std::string ip = resolve_hostname( hostname );
  std::cout << "Resolved to " << ip << std::endl;

  // Create socket
  int fd = create_socket();
  std::cout << "Created socket fd=" << fd << std::endl;

  // Connect to server
  sockaddr_in server_addr {};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons( port );
  if ( inet_pton( AF_INET, ip.c_str(), &server_addr.sin_addr ) != 1 ) {
    close( fd );
    throw std::runtime_error( "inet_pton failed" );
  }

  std::cout << "Connecting to " << ip << ":" << port << "..." << std::endl;
  co_await async_connect( fd, reinterpret_cast<sockaddr*>( &server_addr ), sizeof( server_addr ) );
  std::cout << "Connected!" << std::endl;

  // Build HTTP request
  std::string request = "GET " + path + " HTTP/1.1\r\n";
  request += "Host: " + hostname + "\r\n";
  request += "Connection: close\r\n";
  request += "User-Agent: async-http-client/1.0\r\n";
  request += "\r\n";

  std::cout << "Sending request (" << request.size() << " bytes)..." << std::endl;

  // Send request
  ssize_t bytes_written = 0;
  while ( bytes_written < static_cast<ssize_t>( request.size() ) ) {
    ssize_t n = co_await async_write( fd, request.data() + bytes_written, request.size() - bytes_written );
    bytes_written += n;
  }

  std::cout << "Request sent, reading response..." << std::endl;

  // Read response
  std::string response;
  char buffer[4096];

  while ( true ) {
    ssize_t bytes_read = co_await async_read( fd, buffer, sizeof( buffer ) );
    if ( bytes_read == 0 ) {
      break; // EOF
    }
    response.append( buffer, bytes_read );
  }

  std::cout << "Received " << response.size() << " bytes" << std::endl;

  // Close socket
  co_await async_close( fd );

  co_return response;
}

// Fetch multiple URLs concurrently
Task<void> fetch_multiple_urls()
{
  std::cout << "\n=== Fetching multiple URLs concurrently ===" << std::endl;

  // Spawn multiple concurrent requests
  auto task1 = fetch_url( "example.com", 80, "/" );
  auto task2 = fetch_url( "example.org", 80, "/" );
  auto task3 = fetch_url( "example.net", 80, "/" );

  // Wait for all to complete
  std::cout << "\nFetching URL 1..." << std::endl;
  std::string response1 = co_await task1;
  std::cout << "URL 1 response: " << response1.substr( 0, 200 ) << "..." << std::endl;

  std::cout << "\nFetching URL 2..." << std::endl;
  std::string response2 = co_await task2;
  std::cout << "URL 2 response: " << response2.substr( 0, 200 ) << "..." << std::endl;

  std::cout << "\nFetching URL 3..." << std::endl;
  std::string response3 = co_await task3;
  std::cout << "URL 3 response: " << response3.substr( 0, 200 ) << "..." << std::endl;

  std::cout << "\nAll requests completed!" << std::endl;
}

Task<void> main_task()
{
  try {
    // Single request
    std::cout << "=== Fetching single URL ===" << std::endl;
    std::string response = co_await fetch_url( "example.com", 80, "/" );

    // Print first 500 characters of response
    std::cout << "\nResponse preview:" << std::endl;
    std::cout << response.substr( 0, 500 ) << "..." << std::endl;

    // Wait a bit
    std::cout << "\nWaiting 1 second..." << std::endl;
    co_await sleep_for( time::seconds( 1 ) );

    // Multiple concurrent requests
    co_await fetch_multiple_urls();

  } catch ( const std::exception& e ) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}

int main()
{
  try {
    IoUringRuntime runtime( 256 );
    runtime.spawn( main_task() );
    runtime.run();
  } catch ( const std::exception& e ) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
