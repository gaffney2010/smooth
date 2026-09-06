cmake_minimum_required(VERSION 3.10)
project(smooth CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(smooth INTERFACE)
target_include_directories(smooth INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(smooth_demo src/demo.cpp)
target_link_libraries(smooth_demo PRIVATE smooth)
