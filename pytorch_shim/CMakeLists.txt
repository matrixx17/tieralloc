cmake_minimum_required(VERSION 3.18)
project(tieralloc_torch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Python
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Find PyTorch headers 
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "from torch.utils.cpp_extension import include_paths; print(';'.join(include_paths()))"
    OUTPUT_VARIABLE TORCH_INCLUDE_DIRS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Using Torch include dirs: ${TORCH_INCLUDE_DIRS}")

# Add shim source files
add_library(tieralloc_torch SHARED
    tieralloc_torch.cpp
)

# Tell shim where to find PyTorch and allocator headers
target_include_directories(tieralloc_torch PRIVATE
    ${TORCH_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

# Link shim against tieralloc library
target_link_libraries(tieralloc_torch PRIVATE tieralloc)

# Install shared library to this folder
install(TARGETS tieralloc_torch
  LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/pytorch_shim
  RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/pytorch_shim
  ARCHIVE DESTINATION ${CMAKE_SOURCE_DIR}/pytorch_shim)
