cmake_minimum_required ( VERSION 3.10.0 )

project ( Starter )

include ( etc/build_defs.cmake )
include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/clang_tidy.cmake )

include ( FindOpenSSL )

file ( GLOB SOURCE
  src/util/*
  src/net/*
)

include_directories ( src )

add_library( starter STATIC
  ${SOURCE}
  ${OPENSSL_LIBRARIES}
)

set ( ALL_LIBS
  starter
)

# add_executable ( executable-name src/frontend/executable.cc )
# target_link_libraries( executable-name ${ALL_LIBS} )

# Flags for building static binaries for AWS Lambda
# set ( STATIC_LINK_FLAGS dl z unwind lzma -static -Wl,-allow-multiple-definition
#                         -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
#                         -s )
# if ( CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES ASan )
#   set( STATIC_LINK_FLAGS "" )
# endif()
