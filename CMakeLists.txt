cmake_minimum_required(VERSION 3.28)
project(sigil LANGUAGES CXX)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}:/usr/local/lib64")
set(CMAKE_BUILD_RPATH "${CMAKE_BUILD_RPATH}:/usr/local/lib64")
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD_REQUIRED 1)
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan 1.3 REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

include_directories(include)
add_subdirectory(src)

file(GLOB_RECURSE matrix_files data/*)
file(COPY ${matrix_files} DESTINATION ${CMAKE_BINARY_DIR}/)
