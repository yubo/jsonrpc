/*
 * server.c
 *
 *  Created on: Oct 9, 2012
 *      Author: hmng
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
#include <sys/wait.h>
#include <signal.h>
#include "jsonrpc.h"

#define ADDR "127.0.0.1:1234"	// the port users will be connecting to

struct jrpc_server my_server;

static struct json *say_hello(jrpc_context * ctx, struct json *params,
			       struct json *id)
{
	return json_create_string("Hello!");
}

static struct json *multiply(jrpc_context * ctx, struct json *params,
			      struct json *id)
{
	return json_create_string("Hello!");
}

static struct json *exit_server(jrpc_context * ctx, struct json *params,
				 struct json *id)
{
	jrpc_server_stop(&my_server);
	return json_create_string("Bye!");
}

int main(void)
{
	jrpc_server_init(&my_server, ADDR);
	jrpc_register_procedure(&my_server, say_hello, "sayHello", NULL);
	jrpc_register_procedure(&my_server, exit_server, "exit", NULL);
	jrpc_register_procedure(&my_server, multiply, "multiply", NULL);
	jrpc_server_run(&my_server);
	jrpc_server_destroy(&my_server);
	return 0;
}
