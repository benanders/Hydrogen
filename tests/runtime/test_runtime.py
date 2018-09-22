
#
#  Runtime Tests
#

# Command line arguments:
# 1) Path to folder containing Hydrogen scripts to test
# 2) Path to Hydrogen CLI binary

import os
import sys
import platform
import re

from os.path import join, dirname, basename, isdir, splitext, realpath
from subprocess import Popen, PIPE
from threading import Timer

# Check we have sufficient command line arguments
if len(sys.argv) != 3:
	print("Usage: python test_runtime.py <test scripts folder> <hydrogen cli>")
	sys.exit(1)

# The path to the command line interface which will execute the Hydrogen code
# for us
cli_path = sys.argv[2]

# The amount of time in seconds to let a test case run before killing it
timeout = 2

# Available color codes
COLOR_NONE    = "\x1B[0m"
COLOR_RED     = "\x1B[31m"
COLOR_GREEN   = "\x1B[32m"
COLOR_YELLOW  = "\x1B[33m"
COLOR_BLUE    = "\x1B[34m"
COLOR_MAGENTA = "\x1B[35m"
COLOR_CYAN    = "\x1B[36m"
COLOR_WHITE   = "\x1B[37m"
COLOR_BOLD    = "\x1B[1m"

# Prints some color code to the standard output
def print_color(color):
	if platform.system() != "Windows":
		sys.stdout.write(color)

# Extracts the expected output of a test case from its source code. Returns
# a list of strings, one for each line of expected output, and the expected
# error code for the test
def expected_output(source):
	# We need to find every instance of `//> ` and concatenate them
	# together, separated by newlines
	key = "//> "
	result = []
	line_number = 0
	for line in source.splitlines():
		line_number += 1

		# Find what we're searching for
		found = line.find(key)
		if found == -1:
			continue

		# Append the rest of the line
		index = found + len(key)
		result.append({"content": line[index:], "line": line_number})

	return result

# Runs a test program from its path. Returns the exit code for the process and
# what was written to the standard output. Returns -1 for the error code if the
# process timed out
def run_test(path):
	# Create the process
	proc = Popen([cli_path, path], stdin=PIPE, stdout=PIPE, stderr=PIPE)

	# Kill test cases that take longer than `timeout` seconds
	timed_out = False
	def kill(test_case):
		timed_out = True
		test_case.kill()
	timer = Timer(timeout, kill, [proc])

	# Execute the test case
	exit_code = -1 # Default to failure
	output = None
	error = None
	try:
		timer.start()
		output, error = proc.communicate()
		if not timed_out:
			exit_code = proc.returncode
	finally:
		timer.cancel()

	return (output, error, exit_code)

# Prints an error message to the standard output
def print_error(message):
	print_color(COLOR_RED + COLOR_BOLD)
	sys.stdout.write("[Error] ")
	print_color(COLOR_NONE)
	print(message)

# Validates the output of a test case, returning true if the test was
# successful
def validate(path, expected, output, error, exit_code):
	# Parse the output into lines
	try:
		output = output.decode("utf-8").replace("\r\n", "\n").strip()
	except:
		print_error("Failed to decode output")
		return False

	# Check if the test case timed out
	if exit_code == -1:
		print_error("Timed out")
		return False

	# Check if the test case returned an error
	if exit_code != 0:
		print_error("Test exited with error")
		if len(output) > 0:
			print("Output:")
			print(output)
		if len(error) > 0:
			print("Error:")
			print(error)
		return False

	# Convert output into multiple lines
	output_lines = []
	if len(output) > 0:
		output_lines = output.strip().split("\n")

	# Check output lengths match
	if len(expected) != len(output_lines):
		print_error("Incorrect number of output lines")
		return False

	# Check each line
	for i in range(len(output_lines)):
		expected_line = expected[i]["content"]

		# Check the output matched what was expected
		if output_lines[i] != expected_line:
			line_number = expected[i]["line"]
			print_error("Incorrect output on line " + str(line_number) +
				": expected " + expected_line + ", got " + output_lines[i])
			return False

	# Print passed test case message
	print_color(COLOR_BOLD + COLOR_GREEN)
	sys.stdout.write("[Passed]")
	print_color(COLOR_NONE)
	print("")
	return True

# Executes the runtime test for the Hydrogen code at `path`
def test(path):
	# Print info
	print_color(COLOR_BLUE + COLOR_BOLD)
	sys.stdout.write("[Test] ")
	print_color(COLOR_NONE)

	suite = basename(dirname(path))
	name = splitext(basename(path))[0].replace("_", " ")
	print("Testing " + suite + " > " + name)

	# Open the input file
	input_file = open(path, "r")
	if not input_file:
		print_error("Failed to open file")
		return False

	# Read the contents of the file
	source = input_file.read()
	input_file.close()

	# Extract the expected output for the case
	expected = expected_output(source)

	# Get the output and exit code for the test case
	output, error, exit_code = run_test(path)

	# Validates a test case's output
	return validate(path, expected, output, error, exit_code)

# Tests all Hydrogen files in a directory. Returns the total number of tests,
# and the number of tests passed
def test_dir(path):
	# Count the total number of tests executed, and the number of tests that
	# passed
	total = 0
	passed = 0

	# Iterate over every file in the directory
	files = os.listdir(path)
	for case in files:
		path = join(path, case)
		if isdir(path):
			# Test every subdirectory recursively
			subdir_total, subdir_passed = test_dir(path)
			total += subdir_total
			passed += subdir_passed
		elif splitext(path)[1] == ".hy":
			# Only test files with a file extension `.hy`
			if test(path):
				passed += 1
			total += 1
	return (total, passed)

# Test all files in this directory
total, passed = test_dir(sys.argv[1])

# Add a newline
if total > 0:
	print("")
	print_color(COLOR_BOLD)

# Print number of tests passed
if total == passed:
	# All tests passed
	print_color(COLOR_GREEN)
	sys.stdout.write("[Success] ")
	print_color(COLOR_NONE)
	print("All tests passed!")
else:
	# Some tests failed
	print_color(COLOR_RED)
	sys.stdout.write("[Failure] ")
	print_color(COLOR_NONE)
	print(str(passed) + " of " + str(total) + " tests passed")

	# Exit with a failure status code, to let `make test` know that the runtime
	# tests failed on the whole
	sys.exit(1)
