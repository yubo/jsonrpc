cmake_minimum_required(VERSION 2.6)

INCLUDE(CheckIncludeFiles)

PROJECT(jsonrpc C)
ADD_DEFINITIONS(-O0 -Wall -Werror -Wmissing-declarations -Wno-error=unused-result)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

IF(APPLE)
  INCLUDE_DIRECTORIES(/opt/local/include)
  LINK_DIRECTORIES(/opt/local/lib)
ENDIF()

ADD_LIBRARY(jsonrpc-c SHARED cJSON.c jsonrpc-c.c)
TARGET_LINK_LIBRARIES(jsonrpc-c ev)

ADD_EXECUTABLE(server server.c)
TARGET_LINK_LIBRARIES(server jsonrpc-c)

INSTALL(FILES jsonrpc-c.h
	DESTINATION include
)

INSTALL(TARGETS jsonrpc-c
	LIBRARY DESTINATION lib
)
