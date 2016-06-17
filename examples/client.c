/*
 * Copyright 2016 Xiaomi Corporation. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 *
 * Authors:    Yu Bo <yubo@xiaomi.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "jsonrpc.h"

#define ADDR "127.0.0.1:1234"	// the port users will be connecting to

struct jrpc_client my_client;

int main(void)
{
	char *str_reply;
	struct json *reply;
	struct jrpc_client *client = &my_client;

	int ret = jrpc_client_init(client, ADDR);
	if (ret != 0){
		exit(ret);
	}
	if ((ret = jrpc_client_call(client, "sayHello",
					NULL, &reply)) != 0){
		exit(ret);
	}

	str_reply = json_print(reply);
	printf("%s\n", str_reply);
	free(str_reply);

	return 0;
}
