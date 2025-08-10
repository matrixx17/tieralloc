cmake_minimum_required(VERSION 3.20)
project(tieralloc C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-Wall -Wextra -Wpendantic)

add library(tieralloc SHARED
	src/allocator.cc
	src/throttle.cc
	src/stats.cc
)

target_include_directories(tieralloc PUBLIC ${CMAKE_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
target_link_libraries(tieralloc PRIVATE Threads::Threads dl)

add_executable(tierallocctl tools/tierallocctl.cc)
target_link_libraries(tierallocctl PRIVATE tieralloc)
