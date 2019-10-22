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
option(DEBUG_PRINT_CODE "DEBUG_PRINT_CODE" ON)
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
        include/commons.h
        include/yapl.h
        src/yapl.c
        include/yapl_bytecode_chunk.h
        src/yapl_bytecode_chunk.c
        include/yapl_compiler.h
        src/yapl_compiler.c
        include/yapl_debug.h
        src/yapl_debug.c
        include/yapl_memory_manager.h
        src/yapl_memory_manager.c
        include/yapl_natives.h
        src/yapl_natives.c
        include/yapl_object.h
        src/yapl_object.c
        include/yapl_opcodes.h
        include/yapl_scanner.h
        src/yapl_scanner.c
        include/yapl_table.h
        src/yapl_table.c
        include/yapl_tokens.h
        include/yapl_utils.h
        src/yapl_utils.c
        include/yapl_value.h
        src/yapl_value.c
        include/yapl_vm.h
        src/yapl_vm.c
)

# Link the readline library
find_package(Readline)
if (READLINE_FOUND)
    include_directories(${Readline_INCLUDE_DIR})
    target_link_libraries(yapl ${Readline_LIBRARY})
endif (READLINE_FOUND)