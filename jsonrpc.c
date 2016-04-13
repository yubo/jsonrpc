/*
 * jsonrpc-c.c
 *
 *  Created on: Oct 11, 2012
 *      Author: hmng
 *  
 *  changed by yubo@yubo.org
 *  2016-04-13
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

static int __jrpc_server_start(struct jrpc_server *server);
static void jrpc_procedure_destroy(struct jrpc_procedure *procedure);

struct ev_loop *loop;

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

static int send_response(struct jrpc_connection *conn, char *response)
{
	int fd = conn->fd;
	if (conn->debug_level > 1)
		printf("JSON Response:\n%s\n", response);
	write(fd, response, strlen(response));
	write(fd, "\n", 1);
	return 0;
}

static int send_error(struct jrpc_connection *conn, int code, char *message,
		      struct json *id)
{
	int return_value = 0;
	struct json *result_root = json_create_object();
	struct json *error_root = json_create_object();
	json_add_number_to_object(error_root, "code", code);
	json_add_string_to_object(error_root, "message", message);
	json_add_item_to_object(result_root, "error", error_root);
	json_add_item_to_object(result_root, "id", id);
	char *str_result = json_print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	json_delete(result_root);
	free(message);
	return return_value;
}

static int send_result(struct jrpc_connection *conn, struct json *result,
		       struct json *id)
{
	int return_value = 0;
	struct json *result_root = json_create_object();
	if (result)
		json_add_item_to_object(result_root, "result", result);
	json_add_item_to_object(result_root, "id", id);

	char *str_result = json_print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	json_delete(result_root);
	return return_value;
}

static int invoke_procedure(struct jrpc_server *server,
			    struct jrpc_connection *conn, char *name,
			    struct json *params, struct json *id)
{
	struct json *returned = NULL;
	int procedure_found = 0;
	jrpc_context ctx;
	ctx.error_code = 0;
	ctx.error_message = NULL;
	int i = server->procedure_count;
	while (i--) {
		if (!strcmp(server->procedures[i].name, name)) {
			procedure_found = 1;
			ctx.data = server->procedures[i].data;
			returned =
			    server->procedures[i].function(&ctx, params, id);
			break;
		}
	}
	if (!procedure_found)
		return send_error(conn, JRPC_METHOD_NOT_FOUND,
				  strdup("Method not found."), id);
	else {
		if (ctx.error_code)
			return send_error(conn, ctx.error_code,
					  ctx.error_message, id);
		else
			return send_result(conn, returned, id);
	}
}

static int invoke_procedure_id(struct jrpc_server *server, struct json *method,
			       struct jrpc_connection *conn, struct json *id,
			       struct json *params)
{
	//We have to copy ID because using it on the reply and deleting the response Object will also delete ID
	struct json *id_copy = NULL;
	if (id != NULL)
		id_copy = (id->type == JSON_T_STRING)
		    ? json_create_string(id->valuestring)
		    : json_create_number(id->valueint);
	if (server->debug_level)
		printf("Method Invoked: %s\n", method->valuestring);
	return invoke_procedure(server, conn, method->valuestring, params,
				id_copy);
}

static int eval_request(struct jrpc_server *server,
			struct jrpc_connection *conn, struct json *root)
{
	struct json *method, *params, *id;
	method = json_get_object_item(root, "method");

	if (method != NULL && method->type == JSON_T_STRING) {
		params = json_get_object_item(root, "params");
		if (params == NULL || params->type == JSON_T_ARRAY
		    || params->type == JSON_T_OBJECT) {
			id = json_get_object_item(root, "id");
			if (id == NULL || id->type == JSON_T_STRING
			    || id->type == JSON_T_NUMBER) {
				return invoke_procedure_id(server, method, conn,
							   id, params);
			}

		}
	}
	send_error(conn, JRPC_INVALID_REQUEST,
		   strdup("The JSON sent is not a valid Request object."),
		   NULL);
	return -1;
}

static void close_connection(struct ev_loop *loop, ev_io * w)
{
	ev_io_stop(loop, w);
	close(((struct jrpc_connection *)w)->fd);
	free(((struct jrpc_connection *)w)->buffer);
	free(((struct jrpc_connection *)w));
}

static void connection_cb(struct ev_loop *loop, ev_io * w, int revents)
{
	struct json *root;
	int fd, max_read_size;
	struct jrpc_connection *conn;
	struct jrpc_server *server = (struct jrpc_server *)w->data;
	size_t bytes_read = 0;
	char *str_result, *new_buffer, *end_ptr = NULL;

	//get our 'subclassed' event watcher
	conn = (struct jrpc_connection *)w;
	fd = conn->fd;

	if (conn->pos == (conn->buffer_size - 1)) {
		conn->buffer_size *= 2;
		new_buffer = realloc(conn->buffer, conn->buffer_size);
		if (new_buffer == NULL) {
			perror("Memory error");
			return close_connection(loop, w);
		}
		conn->buffer = new_buffer;
		memset(conn->buffer + conn->pos, 0,
		       conn->buffer_size - conn->pos);
	}
	// can not fill the entire buffer, string must be NULL terminated
	max_read_size = conn->buffer_size - conn->pos - 1;
	if ((bytes_read = read(fd, conn->buffer + conn->pos, max_read_size))
	    == -1) {
		perror("read");
		return close_connection(loop, w);
	}
	if (!bytes_read) {
		// client closed the sending half of the connection
		if (server->debug_level)
			printf("Client closed connection.\n");
		return close_connection(loop, w);
	}

	conn->pos += bytes_read;

	if ((root = json_parse_stream(conn->buffer, &end_ptr)) != NULL) {
		if (server->debug_level > 1) {
			str_result = json_print(root);
			printf("Valid JSON Received:\n%s\n", str_result);
			free(str_result);
		}

		if (root->type == JSON_T_OBJECT) {
			eval_request(server, conn, root);
		}
		//shift processed request, discarding it
		memmove(conn->buffer, end_ptr, strlen(end_ptr) + 2);

		conn->pos = strlen(end_ptr);
		memset(conn->buffer + conn->pos, 0,
		       conn->buffer_size - conn->pos - 1);

		json_delete(root);
	} else {
		// did we parse the all buffer? If so, just wait for more.
		// else there was an error before the buffer's end
		if (end_ptr != (conn->buffer + conn->pos)) {
			if (server->debug_level) {
				printf("INVALID JSON Received:\n---\n%s\n---\n",
				       conn->buffer);
			}
			send_error(conn, JRPC_PARSE_ERROR,
				   strdup("Parse error. Invalid JSON"
					  " was received by the server."),
				   NULL);
			return close_connection(loop, w);
		}
	}

}

static void accept_cb(struct ev_loop *loop, ev_io * w, int revents)
{
	char s[INET6_ADDRSTRLEN];
	struct jrpc_connection *connection_watcher;
	connection_watcher = malloc(sizeof(struct jrpc_connection));
	struct sockaddr_storage their_addr;	// connector's address information
	socklen_t sin_size;
	sin_size = sizeof their_addr;
	connection_watcher->fd = accept(w->fd, (struct sockaddr *)&their_addr,
					&sin_size);
	if (connection_watcher->fd == -1) {
		perror("accept");
		free(connection_watcher);
	} else {
		if (((struct jrpc_server *)w->data)->debug_level) {
			inet_ntop(their_addr.ss_family,
				  get_in_addr((struct sockaddr *)&their_addr),
				  s, sizeof s);
			printf("server: got connection from %s\n", s);
		}
		ev_io_init(&connection_watcher->io, connection_cb,
			   connection_watcher->fd, EV_READ);
		//copy pointer to struct jrpc_server
		connection_watcher->io.data = w->data;
		connection_watcher->buffer_size = 1500;
		connection_watcher->buffer = malloc(1500);
		memset(connection_watcher->buffer, 0, 1500);
		connection_watcher->pos = 0;
		//copy debug_level, struct jrpc_connection has no pointer to struct jrpc_server
		connection_watcher->debug_level =
		    ((struct jrpc_server *)w->data)->debug_level;
		ev_io_start(loop, &connection_watcher->io);
	}
}

int jrpc_server_init(struct jrpc_server *server, char *addr)
{
	loop = EV_DEFAULT;
	return jrpc_server_init_with_ev_loop(server, addr, loop);
}

int jrpc_server_init_with_ev_loop(struct jrpc_server *server,
				  char *addr, struct ev_loop *loop)
{
	memset(server, 0, sizeof(struct jrpc_server));
	server->loop = loop;
	server->addr = addr;
	char *debug_level_env = getenv("JRPC_DEBUG");
	if (debug_level_env == NULL)
		server->debug_level = 0;
	else {
		server->debug_level = strtol(debug_level_env, NULL, 10);
		printf("JSONRPC-C Debug level %d\n", server->debug_level);
	}
	return __jrpc_server_start(server);
}

static int __jrpc_server_start(struct jrpc_server *server)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in sockaddr;
	unsigned int len;
	int yes = 1;
	int rv;
	char buff[128], *host, *port;

	strncpy(buff, server->addr, sizeof(buff) - 1);
	host = buff;
	port = strchr(host, ':');
	if (port == NULL) {
		fprintf(stderr, "err server listen address %s\n", server->addr);
		return 1;
	}
	*port++ = '\0';

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;	// use my IP

	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "err:%s host:%s port:%s\n",
			gai_strerror(rv), host, port);
		return 1;
	}
// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd =
		     socket(p->ai_family, p->ai_socktype, p->ai_protocol))
		    == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt
		    (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
		    == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		len = sizeof(sockaddr);
		if (getsockname(sockfd, (struct sockaddr *)&sockaddr, &len) ==
		    -1) {
			close(sockfd);
			perror("server: getsockname");
			continue;
		}
		//server->port_number = ntohs( sockaddr.sin_port );

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo);	// all done with this structure

	if (listen(sockfd, 5) == -1) {
		perror("listen");
		exit(1);
	}
	if (server->debug_level)
		printf("server: waiting for connections...\n");

	ev_io_init(&server->listen_watcher, accept_cb, sockfd, EV_READ);
	server->listen_watcher.data = server;
	ev_io_start(server->loop, &server->listen_watcher);
	return 0;
}

// Make the code work with both the old (ev_loop/ev_unloop)
// and new (ev_run/ev_break) versions of libev.
#ifdef EVUNLOOP_ALL
#define EV_RUN ev_loop
#define EV_BREAK ev_unloop
#define EVBREAK_ALL EVUNLOOP_ALL
#else
#define EV_RUN ev_run
#define EV_BREAK ev_break
#endif

void jrpc_server_run(struct jrpc_server *server)
{
	EV_RUN(server->loop, 0);
}

int jrpc_server_stop(struct jrpc_server *server)
{
	EV_BREAK(server->loop, EVBREAK_ALL);
	return 0;
}

void jrpc_server_destroy(struct jrpc_server *server)
{
	/* Don't destroy server */
	int i;
	for (i = 0; i < server->procedure_count; i++) {
		jrpc_procedure_destroy(&(server->procedures[i]));
	}
	free(server->procedures);
}

static void jrpc_procedure_destroy(struct jrpc_procedure *procedure)
{
	if (procedure->name) {
		free(procedure->name);
		procedure->name = NULL;
	}
	if (procedure->data) {
		free(procedure->data);
		procedure->data = NULL;
	}
}

int jrpc_register_procedure(struct jrpc_server *server,
			    jrpc_function function_pointer, char *name,
			    void *data)
{
	struct jrpc_procedure *ptr;
	int i = server->procedure_count++;

	if (!server->procedures)
		server->procedures = malloc(sizeof(struct jrpc_procedure));
	else {
		ptr =
		    realloc(server->procedures,
			    sizeof(struct jrpc_procedure) *
			    server->procedure_count);
		if (!ptr)
			return -1;
		server->procedures = ptr;

	}
	if ((server->procedures[i].name = strdup(name)) == NULL)
		return -1;
	server->procedures[i].function = function_pointer;
	server->procedures[i].data = data;
	return 0;
}

int jrpc_deregister_procedure(struct jrpc_server *server, char *name)
{
	/* Search the procedure to deregister */
	int i;
	struct jrpc_procedure *ptr;
	int found = 0;

	if (!server->procedures) {
		fprintf(stderr, "server : procedure '%s' not found\n", name);
		return -1;
	}

	for (i = 0; i < server->procedure_count; i++) {
		if (found)
			server->procedures[i - 1] = server->procedures[i];
		else if (!strcmp(name, server->procedures[i].name)) {
			found = 1;
			jrpc_procedure_destroy(&(server->procedures[i]));
		}
	}

	if (!found)
		return 0;

	server->procedure_count--;

	if (!server->procedure_count) {
		server->procedures = NULL;
	}

	ptr = realloc(server->procedures, sizeof(struct jrpc_procedure)
		      * server->procedure_count);
	if (!ptr) {
		perror("realloc");
		return -1;
	}
	server->procedures = ptr;
	return 0;
}
