cmake_minimum_required(VERSION 2.6)

add_executable(server server.c)
target_link_libraries(server jsonrpc-c)

