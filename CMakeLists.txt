# Run in dir "/yapl"
# Debug build:   cmake -DCMAKE_BUILD_TYPE=Debug CMakeLists.txt
# Release build: cmake -DCMAKE_BUILD_TYPE=Release CMakeLists.txt

# Project basic config
cmake_minimum_required(VERSION 3.13)
project(yapl C)
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

# Adds YAPL debugging macros if requested
if (DEBUG_PRINT_CODE)
    add_compile_definitions(YAPL_DEBUG_PRINT_CODE)
endif (DEBUG_PRINT_CODE)

if (DEBUG_TRACE_EXECUTION)
    add_compile_definitions(YAPL_DEBUG_TRACE_EXECUTION)
endif (DEBUG_TRACE_EXECUTION)

# Build config
add_executable(
        yapl
        src/commons.h
        src/yapl.h
        src/yapl.c
        src/vm/yapl_bytecodechunk.h
        src/vm/yapl_bytecodechunk.c
        src/compiler/yapl_compiler.h
        src/compiler/yapl_compiler.c
        src/lib/yapl_debug.h
        src/lib/yapl_debug.c
        src/lib/yapl_error.h
        src/lib/yapl_error.c
        src/vm/yapl_memmanager.h
        src/vm/yapl_memmanager.c
        src/lib/yapl_natives.h
        src/lib/yapl_natives.c
        src/vm/yapl_object.h
        src/vm/yapl_object.c
        src/vm/yapl_opcodes.h
        src/compiler/yapl_scanner.h
        src/compiler/yapl_scanner.c
        src/lib/yapl_table.h
        src/lib/yapl_table.c
        src/compiler/yapl_tokens.h
        src/utils/yapl_utils.h
        src/utils/yapl_utils.c
        src/vm/yapl_value.h
        src/vm/yapl_value.c
        src/vm/yapl_vm.h
        src/vm/yapl_vm.c
)

# Link the readline library
find_package(Readline)
if (READLINE_FOUND)
    include_directories(${Readline_INCLUDE_DIR})
    target_link_libraries(yapl ${Readline_LIBRARY})
endif (READLINE_FOUND)