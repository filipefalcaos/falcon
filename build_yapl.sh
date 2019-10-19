#!/usr/bin/env bash

# Remove CMake build files
remove_build_files() {
  rm -rf CMakeFiles/
  rm -f cmake_install.cmake
  rm -f CMakeCache.txt
  rm -f Makefile
  rm -f yapl.cbp
}

# Build YAPL
# Debugging: cmake CMakeLists.txt -DDEBUG_PRINT_CODE=ON -DDEBUG_TRACE_EXECUTION=ON
remove_build_files
printf "Building YAPL with CMake...\n"
cmake CMakeLists.txt
printf "\nRunning Make...\n"
make
remove_build_files

# Start the YAPL's REPL
printf "\nStarting YAPL's REPL...\n"
./bin/yapl
