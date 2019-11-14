#!/usr/bin/env bash

# Removes CMake build files
remove_build_files() {
  rm -rf CMakeFiles/
  rm -rf cmake-build-debug/
  rm -f cmake_install.cmake
  rm -f CMakeCache.txt
  rm -f Makefile
  rm -f yapl.cbp
}

# Builds YAPL
# Debugging: cmake CMakeLists.txt -DDEBUG_PRINT_CODE=ON -DDEBUG_TRACE_EXECUTION=ON
remove_build_files
printf "Building YAPL with CMake...\n"
cmake -DCMAKE_BUILD_TYPE=Debug CMakeLists.txt || exit 1
printf "\nRunning Make...\n"
make || exit 1
remove_build_files || exit 1

# Starts the YAPL's REPL
printf "\nStarting YAPL's REPL...\n"
./bin/yapl
