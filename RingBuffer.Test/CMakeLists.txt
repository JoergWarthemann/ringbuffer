# Specify the minimum version for CMake.
cmake_minimum_required(VERSION 3.8.0)
# Project's name.
project(ringbuffer.test VERSION 0.1.0)

enable_testing()

# Set the output folder where your program will be created.
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
# Set the output folder for executable files.
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# Use C++17.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the main include directory.
include_directories(
	include
	../RingBuffer/include
	)

# Prepare cmake to search for catch2 tests.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/lib/Catch2/cmake/modules)

# Prepare catch library for other executables.
set(CATCH_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/lib/Catch2/)
include_directories(${INCLUDE_DIRECTORIES} ${CATCH_INCLUDE_DIR}) 

file(GLOB SOURCE_FILES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Search for catch2 tests.
include(ParseAndAddCatchTests)
ParseAndAddCatchTests(${PROJECT_NAME})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
