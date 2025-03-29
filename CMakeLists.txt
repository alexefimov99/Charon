cmake_minimum_required(VERSION 3.10)
project(charon)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

find_package(Boost 1.66 REQUIRED COMPONENTS system thread)

if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_WIN32_WINNT=0x0601)
    add_compile_definitions(_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

add_executable(charon tcp_server.cpp)

target_include_directories(charon PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(charon PRIVATE ${Boost_LIBRARIES})

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(charon PRIVATE Threads::Threads)
endif()

install(TARGETS charon
        RUNTIME DESTINATION bin)

if(WIN32)
    target_compile_definitions(charon PRIVATE _WIN32_WINNT=0x0601)
endif()

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)
target_include_directories(charon PRIVATE ${CMAKE_CURRENT_BINARY_DIR})