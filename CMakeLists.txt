
# CMakeLists.txt
# By Ben Anderson
# July 2018

cmake_minimum_required(VERSION 3.6)
project(hydrogen)
set(CMAKE_C_STANDARD 99)

# Turn on all warnings for all C code
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

# Turn off optimisations and add debugging symbols when in debug mode
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")

# Turn on all optimisations when in release mode
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# Remove the annoying warning about type casting a `char *` string to a `const
# char *` in the C++ tests
set(CMAKE_CXX_FLAGS "-Wno-c++11-compat-deprecated-writable-strings")

# We split the interpreter up into a core library (hyvm) and a separate
# executable linked against this library (hydrogen) so that we can link the
# C++ tests against the core library, rather than having to re-compile all the
# source files again.
#
# Create the core VM library
add_library(hyvm STATIC 
	src/vm.c src/vm.h
	src/parser.c src/parser.h
	src/lexer.c src/lexer.h
	src/bytecode.h src/value.h
	src/util.c src/util.h)

# Create the CLI executable
add_executable(hydrogen src/main.c)
target_link_libraries(hydrogen hyvm)

# The tests use the C++ Google Test framework
add_subdirectory(tests/gtest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include src)

# Macro for adding a test suite
macro(test name)
	add_executable(test_${name} tests/test_${name}.cpp)
	target_link_libraries(test_${name} hyvm gtest gtest_main)
	add_test(test_${name} test_${name})
endmacro()

# Test suites
test(lexer)
test(parser)

# The runtime tests are run using a Python script, because that makes it easier
# to parse the expected output of the test straight from the .hy test file.
find_package(Python REQUIRED COMPONENTS Interpreter)

# Add runtime tests
#
# To run just the runtime tests, execute the following command from inside the
# build folder:
#   $ python ../tests/runtime/test_runtime.py ../tests/runtime ./hydrogen
add_test(NAME test_runtime
	COMMAND ${Python_EXECUTABLE}
	${CMAKE_SOURCE_DIR}/tests/runtime/test_runtime.py
	${CMAKE_SOURCE_DIR}/tests/runtime
	${CMAKE_BINARY_DIR}/hydrogen)
