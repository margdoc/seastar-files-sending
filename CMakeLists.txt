cmake_minimum_required(VERSION 3.26)
project(FilesSending)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)
add_compile_options("-fcoroutines")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package (Valgrind REQUIRED)
find_package (Seastar REQUIRED)


add_executable (client
    client.cc)
  
target_link_libraries (client
    Seastar::seastar)


add_executable (server
    server.cc)
    
target_link_libraries (server
    Seastar::seastar)
