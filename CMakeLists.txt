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

# Build config
add_executable(
        yapl
        include/commons.h
        include/yapl.h
        src/yapl.c
        include/yapl_utils.h
        src/yapl_utils.c
)

# Link the readline library
find_package(Readline)
if (READLINE_FOUND)
    include_directories(${Readline_INCLUDE_DIR})
    target_link_libraries(yapl ${Readline_LIBRARY})
endif (READLINE_FOUND)