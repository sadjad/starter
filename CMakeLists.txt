cmake_minimum_required ( VERSION 3.10.0 )

project ( Starter )

include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/clang_tidy.cmake )

include ( FindOpenSSL )

include_directories ( src )

file ( GLOB SOURCE
  src/util/*
  src/net/*
)

file ( GLOB ASYNC_SOURCE
  src/async/*.cc
)

add_library ( starter STATIC ${SOURCE} )
add_library ( async STATIC ${ASYNC_SOURCE} )

set ( ALL_LIBS
  starter
  ${OPENSSL_LIBRARIES}
)

set ( ASYNC_LIBS
  async
  uring
)

add_executable ( prog src/frontend/prog.cc )
target_link_libraries ( prog ${ALL_LIBS} )

# Async examples
add_executable ( echo_server_async src/examples/echo_server_async.cc )
target_link_libraries ( echo_server_async ${ASYNC_LIBS} )

add_executable ( http_client_async src/examples/http_client_async.cc )
target_link_libraries ( http_client_async ${ASYNC_LIBS} )
