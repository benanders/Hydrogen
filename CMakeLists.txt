
# CMakeLists.txt
# By Ben Anderson
# July 2018

cmake_minimum_required(VERSION 3.6)
project(hydrogen)
set(CMAKE_C_STANDARD 99)

# Set the C compiler's debug and release flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS "-Wno-c++11-compat-deprecated-writable-strings")

# Source files for the core library
set(VM_SOURCES
	src/include/hydrogen.h
	src/vm/vm.c src/vm/vm.h
	src/vm/parser.c src/vm/parser.h
	src/vm/lexer.c src/vm/lexer.h
	src/vm/bytecode.h)

# Create the core VM library
add_library(hyvm STATIC ${VM_SOURCES})
target_include_directories(hyvm PUBLIC src/include)

# Create the CLI executable
set(CLI_SOURCES
	src/include/hydrogen.h
	src/cli/main.c)
add_executable(hydrogen ${CLI_SOURCES})
target_link_libraries(hydrogen hyvm)

# Configure Google Test framework
add_subdirectory(tests/gtest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include src/include src)

# Macro for adding a test suite
macro(test name)
	add_executable(test_${name} tests/test_${name}.cpp)
	target_link_libraries(test_${name} hyvm gtest gtest_main)
	add_test(test_${name} test_${name})
endmacro()

# Test suites
test(lexer)
