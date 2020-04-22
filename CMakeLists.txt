# Debug build:   cmake -DCMAKE_BUILD_TYPE=Debug [options] ... CMakeLists.txt
# Release build: cmake -DCMAKE_BUILD_TYPE=Release [options] ... CMakeLists.txt

# Project basic config
cmake_minimum_required(VERSION 3.12)
project(falcon C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Sets cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Output config
set(CMAKE_BINARY_DIR .)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Adds C compiler and OS information
add_compile_definitions(CMAKE_C_COMPILER_ID="${CMAKE_C_COMPILER_ID}")
add_compile_definitions(CMAKE_C_COMPILER_VERSION="${CMAKE_C_COMPILER_VERSION}")
add_compile_definitions(CMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}")

# Build options
# Enables garbage collector stressing
option(STRESS_GC "Stress the garbage collector by triggering a run every time an object is allocated" OFF)

if (STRESS_GC)
    message("-- Flag STRESS_GC is set to ON")
    add_compile_definitions(FALCON_STRESS_GC)
endif (STRESS_GC)

# Enables the usage of the readline library
option(USE_READLINE "Define whether the readline library should be used on the REPL or not" ON)

if (USE_READLINE)
    message("-- Flag USE_READLINE is set to ON")
    add_compile_definitions(FALCON_USE_READLINE)
endif (USE_READLINE)

# Build config
add_executable(
    falcon
    src/falcon.h
    src/falcon.c

    # Core
    src/core/fl-bcode.h
    src/core/fl-bcode.c
    src/core/fl-compiler.h
    src/core/fl-compiler.c
    src/core/fl-debug.h
    src/core/fl-debug.c
    src/core/fl-gc.h
    src/core/fl-gc.c
    src/core/fl-mem.h
    src/core/fl-mem.c
    src/core/fl-object.h
    src/core/fl-object.c
    src/core/fl-opcodes.h
    src/core/fl-scanner.h
    src/core/fl-scanner.c
    src/core/fl-tokens.h
    src/core/fl-value.h
    src/core/fl-value.c
    src/core/fl-vm.h
    src/core/fl-vm.c

    # Standard Lib
    src/lib/fl-baselib.h
    src/lib/fl-baselib.c
    src/lib/fl-iolib.h
    src/lib/fl-iolib.c
    src/lib/fl-listlib.h
    src/lib/fl-listlib.c
    src/lib/fl-maplib.h
    src/lib/fl-maplib.c
    src/lib/fl-mathlib.h
    src/lib/fl-mathlib.c
    src/lib/fl-strlib.h
    src/lib/fl-strlib.c
    src/lib/fl-syslib.h
    src/lib/fl-syslib.c
)

# Links the <math.h> library
target_link_libraries(falcon m)

# Links the readline library
find_package(Readline)
if (READLINE_FOUND)
    add_compile_definitions(FALCON_READLINE_AVAILABLE)
    include_directories(${READLINE_INCLUDE_DIR})
    target_link_libraries(falcon ${READLINE_LIBRARY})
endif (READLINE_FOUND)
