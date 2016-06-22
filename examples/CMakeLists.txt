#
# Copyright 2016 Xiaomi Corporation. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
#
# Authors:    Yu Bo <yubo@xiaomi.com>
#
cmake_minimum_required(VERSION 2.6)

add_executable(server server.c)
target_link_libraries(server jsonrpc m)

add_executable(client client.c)
target_link_libraries(client jsonrpc m)

