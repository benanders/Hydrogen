
#
#  Hydrogen Build File
#

cmake_minimum_required(VERSION 3.6)
project(hydrogen)
set(CMAKE_C_STANDARD 99)

# Set the C compiler's debug and release flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

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
