cmake_minimum_required(VERSION 2.6)

include(CheckIncludeFiles)

project(jsonrpc C)
add_definitions(-O0 -Wall -Werror -Wmissing-declarations -Wno-error=unused-result -I${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

if(APPLE)
  INCLUDE_DIRECTORIES(/opt/local/include)
  LINK_DIRECTORIES(/opt/local/lib)
endif()

add_library(jsonrpc SHARED json.c jsonrpc.c)
target_link_libraries(jsonrpc ev)

install(FILES jsonrpc.h
	DESTINATION include
)

install(TARGETS jsonrpc
	LIBRARY DESTINATION lib
)

add_subdirectory(examples)
