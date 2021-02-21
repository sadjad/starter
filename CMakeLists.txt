cmake_minimum_required ( VERSION 2.8.5 )
cmake_policy ( SET CMP0054 NEW )
project ( Starter )

include ( etc/build_defs.cmake )
include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/clang_tidy.cmake )

file ( GLOB SOURCE
  src/util/*
  src/net/*
  src/remote/*
  src/schedulers/*
  src/simulator/*
  src/storage/*
)

include_directories ( src )

add_library( starter STATIC
  ${SOURCE}
)

set ( ALL_LIBS
  starter
)
