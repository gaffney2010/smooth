cmake_minimum_required(VERSION 3.14)
project(smooth CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(smooth INTERFACE)
target_include_directories(smooth INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(smooth_demo src/demo.cpp)
target_link_libraries(smooth_demo PRIVATE smooth)

find_package(SQLite3 REQUIRED)
add_executable(smooth_profiles src/profile_runner.cpp)
target_link_libraries(smooth_profiles PRIVATE smooth SQLite::SQLite3)

enable_testing()
add_executable(smooth_tests tests/test_smooth.cpp)
target_link_libraries(smooth_tests PRIVATE smooth)
add_test(NAME smooth_tests COMMAND smooth_tests)
