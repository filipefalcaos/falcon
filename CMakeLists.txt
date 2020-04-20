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

    # Compiler
    src/compiler/falcon_compiler.h
    src/compiler/falcon_compiler.c
    src/compiler/falcon_scanner.h
    src/compiler/falcon_scanner.c
    src/compiler/falcon_tokens.h

    # Standard Lib
    src/lib/falcon_io.h
    src/lib/falcon_io.c
    src/lib/falcon_list.h
    src/lib/falcon_list.c
    src/lib/falcon_map.h
    src/lib/falcon_map.c
    src/lib/falcon_natives.h
    src/lib/falcon_natives.c
    src/lib/falcon_string.h
    src/lib/falcon_string.c

    # Virtual Machine
    src/vm/falcon_bytecode.h
    src/vm/falcon_bytecode.c
    src/vm/falcon_debug.h
    src/vm/falcon_debug.c
    src/vm/falcon_gc.h
    src/vm/falcon_gc.c
    src/vm/falcon_memory.h
    src/vm/falcon_memory.c
    src/vm/falcon_object.h
    src/vm/falcon_object.c
    src/vm/falcon_opcodes.h
    src/vm/falcon_value.h
    src/vm/falcon_value.c
    src/vm/falcon_vm.h
    src/vm/falcon_vm.c
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
