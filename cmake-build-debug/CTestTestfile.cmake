# CMake generated Testfile for 
# Source directory: /Users/benanderson/dev/hydrogen
# Build directory: /Users/benanderson/dev/hydrogen/cmake-build-debug
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_lexer "test_lexer")
add_test(test_parser "test_parser")
subdirs("tests/gtest")
