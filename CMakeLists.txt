cmake_minimum_required(VERSION 3.24)

project(AutoSprint)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-dynamic-class-memaccess -Wno-microsoft-goto -Wno-delete-incomplete /EHsc /Ob0)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/MP /Ob1)
endif()

# Add definitions
add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(NOMINMAX)

# Include header files
include_directories(include)

# Link the libraries
link_directories(libs)

add_library(AutoSprint SHARED dllmain.cpp)