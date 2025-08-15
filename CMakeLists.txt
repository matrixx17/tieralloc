cmake_minimum_required(VERSION 3.20)
project(tieralloc C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

add_library(tieralloc SHARED
    src/allocator.cc
    src/throttle.cc
    src/stats.cc
    src/interpose.cc
    src/policy.cc
)

target_include_directories(tieralloc PUBLIC ${CMAKE_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
target_link_libraries(tieralloc PRIVATE Threads::Threads dl)

find_library(LIBNUMA numa)
if(LIBNUMA)
  target_link_libraries(tieralloc PRIVATE ${LIBNUMA})
  target_compile_definitions(tieralloc PRIVATE TA_HAVE_LIBNUMA=1 TA_HAVE_NUMA=1)
endif()

add_executable(tierallocctl tools/tierallocctl.cc)
target_link_libraries(tierallocctl PRIVATE tieralloc)

add_executable(bench_alloc benchmark/bench_alloc.cc)
target_link_libraries(bench_alloc PRIVATE tieralloc)

add_executable(bench_roll benchmark/bench_roll.cc)
target_link_libraries(bench_roll PRIVATE tieralloc)

add_subdirectory(pytorch_shim)
