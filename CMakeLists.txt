cmake_minimum_required(VERSION 3.20)
project(print-trace)

# Set this to a valid LLVM installation dir
set(LT_LLVM_INSTALL_DIR "" CACHE PATH "LLVM installation directory")

# Add the location of LLVMConfig.cmake to CMake search paths (so that
# find_package can locate it)
list(APPEND CMAKE_PREFIX_PATH "${LT_LLVM_INSTALL_DIR}/lib/cmake/llvm/")

find_package(LLVM CONFIG)
if("${LLVM_VERSION_MAJOR}" VERSION_LESS 18)
  message(FATAL_ERROR "Found LLVM ${LLVM_VERSION_MAJOR}, but need LLVM 18 or above")
endif()

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 17 CACHE STRING "")

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# Compile print_func.c
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/print_func.o
  COMMAND clang -c ${CMAKE_CURRENT_SOURCE_DIR}/print_func.c -o ${CMAKE_CURRENT_BINARY_DIR}/print_func.o
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/print_func.c
  COMMENT "Compiling print_func.c"
)

add_library(Print_trace SHARED Print_trace.cpp ${CMAKE_CURRENT_BINARY_DIR}/print_func.o)

# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(Print_trace
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
