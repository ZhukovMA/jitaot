
cmake_minimum_required(VERSION 3.16)
project(ir_full_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# header-only "libraries"
add_library(ir INTERFACE)
target_include_directories(ir INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(analysis INTERFACE)
target_include_directories(analysis INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(analysis INTERFACE ir)

add_executable(tests
    main.cpp
    tests/test_dfs_rpo_idom.cpp
    tests/test_loops.cpp
)

target_link_libraries(tests PRIVATE analysis ir)

