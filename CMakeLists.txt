cmake_minimum_required(VERSION 3.16)
project(ir_full_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ir INTERFACE)
target_include_directories(ir INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(analysis INTERFACE)
target_include_directories(analysis INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(analysis INTERFACE ir)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

add_executable(tests
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${TEST_SOURCES}
)

target_link_libraries(tests PRIVATE analysis ir)