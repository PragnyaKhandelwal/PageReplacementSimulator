cmake_minimum_required(VERSION 3.10)
project(PageReplacementSimulator VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB_RECURSE PRS_SOURCES CONFIGURE_DEPENDS src/core/*.cpp src/algorithms/*.cpp
     src/analysis/*.cpp src/io/*.cpp src/ui/*.cpp)

add_library(prs_core STATIC ${PRS_SOURCES})
target_include_directories(prs_core PUBLIC include)
if(MSVC)
  target_compile_options(prs_core PRIVATE /W4)
else()
  target_compile_options(prs_core PRIVATE -Wall -Wextra -pedantic)
endif()

add_executable(pagesim src/main.cpp)
target_link_libraries(pagesim PRIVATE prs_core)

add_executable(pagesim_tests tests/test_main.cpp)
target_link_libraries(pagesim_tests PRIVATE prs_core)

enable_testing()
add_test(NAME unit_tests COMMAND pagesim_tests)
