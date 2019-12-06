# Run in dir "/falcon"
# Debug build:   cmake -DCMAKE_BUILD_TYPE=Debug CMakeLists.txt
# Release build: cmake -DCMAKE_BUILD_TYPE=Release CMakeLists.txt

# Project basic config
cmake_minimum_required(VERSION 3.12)
project(falcon C)
set(CMAKE_C_STANDARD 99)

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
option(DEBUG_PRINT_CODE "DEBUG_PRINT_CODE" OFF)
option(DEBUG_TRACE_EXECUTION "DEBUG_TRACE_EXECUTION" OFF)
option(DEBUG_STRESS_GC "DEBUG_STRESS_GC" OFF)
option(DEBUG_LOG_GC "DEBUG_LOG_MEMORY" OFF)
option(DEBUG_LOG_GC "DEBUG_LOG_GC" ON)

# Adds Falcon debugging macros if requested
if (DEBUG_PRINT_CODE)
    add_compile_definitions(FALCON_DEBUG_PRINT_CODE)
endif (DEBUG_PRINT_CODE)

if (DEBUG_TRACE_EXECUTION)
    add_compile_definitions(FALCON_DEBUG_TRACE_EXECUTION)
endif (DEBUG_TRACE_EXECUTION)

if (DEBUG_STRESS_GC)
    add_compile_definitions(FALCON_DEBUG_STRESS_GC)
endif (DEBUG_STRESS_GC)

if (DEBUG_LOG_MEMORY)
    add_compile_definitions(FALCON_DEBUG_LOG_MEMORY)
endif (DEBUG_LOG_MEMORY)

if (DEBUG_LOG_GC)
    add_compile_definitions(FALCON_DEBUG_LOG_GC)
endif (DEBUG_LOG_GC)

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
        src/lib/falcon_debug.h
        src/lib/falcon_debug.c
        src/lib/falcon_error.h
        src/lib/falcon_error.c
        src/lib/falcon_natives.h
        src/lib/falcon_natives.c
        src/lib/falcon_table.h
        src/lib/falcon_table.c
        src/lib/falcon_math.h
        src/lib/falcon_math.c
        src/lib/falcon_io.h
        src/lib/falcon_io.c
        src/lib/falcon_string.h
        src/lib/falcon_string.c

        # Virtual Machine
        src/vm/falcon_bytecode.h
        src/vm/falcon_bytecode.c
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
