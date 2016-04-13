cmake_minimum_required(VERSION 2.6)

include(CheckIncludeFiles)

project(jsonrpc C)
add_definitions(-O0 -Wall -Werror -Wmissing-declarations -Wno-error=unused-result -I${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

if(APPLE)
  INCLUDE_DIRECTORIES(/opt/local/include)
  LINK_DIRECTORIES(/opt/local/lib)
endif()

add_library(jsonrpc-c SHARED cJSON.c jsonrpc-c.c)
target_link_libraries(jsonrpc-c ev)

install(FILES jsonrpc-c.h
	DESTINATION include
)

install(TARGETS jsonrpc-c
	LIBRARY DESTINATION lib
)

add_subdirectory(examples)
