# Run in dir "/falcon"
# Debug build:   cmake -DCMAKE_BUILD_TYPE=Debug CMakeLists.txt
# Release build: cmake -DCMAKE_BUILD_TYPE=Release CMakeLists.txt

# Project basic config
cmake_minimum_required(VERSION 3.12)
project(falcon C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Set cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Output config
set(CMAKE_BINARY_DIR .)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add C compiler and OS information
add_compile_definitions(CMAKE_C_COMPILER_ID="${CMAKE_C_COMPILER_ID}")
add_compile_definitions(CMAKE_C_COMPILER_VERSION="${CMAKE_C_COMPILER_VERSION}")
add_compile_definitions(CMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}")

# Debugging options
option(DEBUG_LEVEL_02 "DEBUG_LEVEL_02" OFF)
option(DEBUG_STRESS_GC "DEBUG_STRESS_GC" OFF)

# Adds Falcon debugging macros if requested
# Sets LEVEL_01 flag
if (DEBUG_LEVEL_01)
    add_compile_definitions(FALCON_DEBUG_LEVEL_01)
endif (DEBUG_LEVEL_01)

# Sets LEVEL_02 flag only if not in LEVEL_01 already
if (DEBUG_LEVEL_02 AND (NOT DEBUG_LEVEL_01))
    add_compile_definitions(FALCON_DEBUG_LEVEL_02)
endif (DEBUG_LEVEL_02 AND (NOT DEBUG_LEVEL_01))

# Sets the STRESS_GC flag
if (DEBUG_STRESS_GC)
    add_compile_definitions(FALCON_DEBUG_STRESS_GC)
endif (DEBUG_STRESS_GC)

# Build config
add_executable(
    falcon
    src/commons.h
    src/falcon.h
    src/falcon.c

    # Compiler
    src/compiler/falcon_compiler.h
    src/compiler/falcon_compiler.c
    src/compiler/falcon_scanner.h
    src/compiler/falcon_scanner.c
    src/compiler/falcon_tokens.h

    # Standard Lib
    src/lib/falcon_error.h
    src/lib/falcon_error.c
    src/lib/falcon_io.h
    src/lib/falcon_io.c
    src/lib/falcon_list.h
    src/lib/falcon_list.c
    src/lib/falcon_natives.h
    src/lib/falcon_natives.c
    src/lib/falcon_string.h
    src/lib/falcon_string.c
    src/lib/falcon_table.h
    src/lib/falcon_table.c

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

# Link the <math.h> library
target_link_libraries(falcon m)

# Link the readline library
find_package(Readline)
if (READLINE_FOUND)
    add_compile_definitions(FALCON_READLINE_AVAILABLE)
    include_directories(${READLINE_INCLUDE_DIR})
    target_link_libraries(falcon ${READLINE_LIBRARY})
endif (READLINE_FOUND)
