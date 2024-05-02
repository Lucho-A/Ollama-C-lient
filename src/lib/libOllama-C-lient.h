/*
 ============================================================================
 Name        : libOllama-C-lient.h
 Author      : L. (lucho-a.github.io)
 Version     : 0.0.1
 Created on	 : 204/04/19
 Copyright   : GNU General Public License v3.0
 Description : Header file
 ============================================================================
 */

#ifndef HEADERS_LIBOLLAMA_C_LIENT_H_
#define HEADERS_LIBOLLAMA_C_LIENT_H_

#include <stdio.h>
#include <stdlib.h>

#define RETURN_ERROR 								-1
#define RETURN_OK 									0

#define LIBOLLAMA_C_LIENT_NAME 						"libGPT"
#define LIBOLLAMA_C_LIENT_MAJOR_VERSION				"0"
#define LIBOLLAMA_C_LIENT_MINOR_VERSION				"0"
#define LIBOLLAMA_C_LIENT_MICRO_VERSION				"1"
#define LIBOLLAMA_C_LIENT_VERSION					PROGRAM_MAJOR_VERSION"."PROGRAM_MINOR_VERSION"."PROGRAM_MICRO_VERSION
#define LIBOLLAMA_C_LIENT_DESCRIPTION				"C library for interacting with Ollama server"

#define DBG											printf("\nWTFFF?!?!\n");

#define OLLAMA_SERVER_ADDR							"127.0.0.1"
#define OLLAMA_SERVER_PORT							443

#define	RESPONSE_SPEED								15000

#define SOCKET_CONNECT_TIMEOUT_S					5
#define SOCKET_SEND_TIMEOUT_MS						5000
#define SOCKET_RECV_TIMEOUT_MS						120000

#define MAX_HISTORY_CONTEXT							3
#define TEMP										0.5
#define MAX_TOKENS									2048
#define NUM_CTX										2048

typedef enum{
	FALSE=0,
	TRUE
}bool;

enum errors{
	ERR_INIT_ERROR=-50,
	ERR_MALLOC_ERROR,
	ERR_REALLOC_ERROR,
	ERR_GETTING_HOST_INFO_ERROR,
	ERR_SOCKET_CREATION_ERROR,
	ERR_SOCKET_CONNECTION_TIMEOUT_ERROR,
	ERR_SSL_CONTEXT_ERROR,
	ERR_SSL_CERT_NOT_FOUND,
	ERR_SSL_FD_ERROR,
	ERR_SSL_CONNECT_ERROR,
	ERR_SOCKET_SEND_TIMEOUT_ERROR,
	ERR_SENDING_PACKETS_ERROR,
	ERR_POLLIN_ERROR,
	ERR_SOCKET_RECV_TIMEOUT_ERROR,
	ERR_RECV_TIMEOUT_ERROR,
	ERR_RECEIVING_PACKETS_ERROR,
	ERR_RESPONSE_MESSAGE_ERROR,
	ERR_ZEROBYTESRECV_ERROR,
	ERR_MODEL_FILE_NOT_FOUND,
	ERR_CONTEXT_FILE_NOT_FOUND,
	ERR_CERT_FILE_NOT_FOUND,
	ERR_OPENING_FILE_ERROR,
	ERR_OPENING_ROLE_FILE_ERROR,
	ERR_NO_HISTORY_CONTEXT_ERROR,
	ERR_UNEXPECTED_JSON_FORMAT_ERROR,
	ERR_CONTEXT_MSGS_ERROR,
	ERR_NULL_STRUCT_ERROR,
	ERR_SERVICE_UNAVAILABLE,
	ERR_LOADING_MODEL,
	ERR_UNLOADING_MODEL,
	ERR_SERVER_ADDR,
	ERR_PORT,
	ERR_TEMP,
	ERR_MAX_MSG_CTX,
	ERR_MAX_TOKENS_CTX,
	ERR_MAX_TOKENS
};

typedef struct _ocl OCl;

struct Colors{
	char *yellow;
	char *h_green;
	char *h_red;
	char *h_cyan;
	char *h_white;
	char *def;
};

extern struct Colors Colors;
extern bool canceled;

int OCl_set_server_addr(OCl *, char *);
int OCl_set_server_port(OCl *, char *);
int OCl_set_modelfile(OCl *, char *);
int OCl_set_contextfile(OCl *, char *);
int OCl_set_show_resp_info(OCl *, bool);

void OCl_init_colors(bool);

int OCl_init();
OCl * OCl_get_instance();
int OCl_free(OCl *);
int OCl_flush_context();
int OCl_load_modelfile(OCl *, char *);
int OCl_load_model(OCl *, bool);
int OCl_send_chat(OCl *, char *);
int OCl_check_service_status(OCl *);
int OCl_import_context(OCl *);
char * OCL_error_handling(int);

#endif /* HEADERS_LIBOLLAMA_C_LIENT_H_ */
