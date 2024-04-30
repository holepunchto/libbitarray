cmake_minimum_required(VERSION 3.25)

project(bitarray C)

if(NOT TARGET intrusive)
  add_subdirectory(vendor/libintrusive EXCLUDE_FROM_ALL)
endif()

add_library(bitarray OBJECT)

set_target_properties(
  bitarray
  PROPERTIES
  C_STANDARD 99
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  bitarray
  INTERFACE
    include/bitarray.h
  PRIVATE
    src/bitarray.c
)

target_include_directories(
  bitarray
  PUBLIC
    include
)

target_link_libraries(
  bitarray
  PUBLIC
    intrusive
)

add_library(bitarray_shared SHARED)

set_target_properties(
  bitarray_shared
  PROPERTIES
  OUTPUT_NAME bitarray
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_link_libraries(
  bitarray_shared
  PUBLIC
    bitarray
    intrusive_shared
)

add_library(bitarray_static STATIC)

set_target_properties(
  bitarray_static
  PROPERTIES
  OUTPUT_NAME bitarray
  PREFIX lib
)

target_link_libraries(
  bitarray_static
  PUBLIC
    bitarray
    intrusive_static
)

install(TARGETS bitarray_shared bitarray_static)

install(FILES include/bitarray.h DESTINATION include)

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()

  add_subdirectory(test)
endif()
