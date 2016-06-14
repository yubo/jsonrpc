#
# Copyright 2016 Xiaomi Corporation. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
#
# Authors:    Yu Bo <yubo@xiaomi.com>
#
cmake_minimum_required(VERSION 2.6)

include(CheckIncludeFiles)

project(jsonrpc C)
add_definitions(-O0 -Wall -Werror -Wmissing-declarations -Wno-error=unused-result -I${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

if(APPLE)
  INCLUDE_DIRECTORIES(/opt/local/include)
  LINK_DIRECTORIES(/opt/local/lib)
endif()

set(SOURCES json.c jsonrpc.c)

add_library(jsonrpc STATIC ${SOURCES})
target_link_libraries(jsonrpc ev)

add_subdirectory(examples)
