# Run in dir "/falcon"
# Debug build:   cmake -DCMAKE_BUILD_TYPE=Debug [options] ... CMakeLists.txt
# Release build: cmake -DCMAKE_BUILD_TYPE=Release [options] ... CMakeLists.txt

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

# CMake options
option(STRESS_GC "Stress the garbage collector by triggering a run every time an object is allocated" OFF)

# Sets the FALCON_STRESS_GC flag if requested
if (STRESS_GC)
    message("-- Flag STRESS_GC set to ON")
    add_compile_definitions(FALCON_STRESS_GC)
endif (STRESS_GC)

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
    src/lib/falcon_error.h
    src/lib/falcon_error.c
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
