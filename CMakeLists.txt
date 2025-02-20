cmake_minimum_required(VERSION 3.10)
project(CharonServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0167 NEW)
find_package(Boost REQUIRED COMPONENTS system thread)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found! Install Boost and try again.")
endif()

include_directories(${Boost_INCLUDE_DIRS})

add_executable(CharonServer tcp_server.cpp tcp_connection.cpp)
target_link_libraries(CharonServer Boost::system Boost::thread)

add_executable(CharonClient tcp_client.cpp tcp_connection.cpp)
target_link_libraries(CharonClient Boost::system Boost::thread)
