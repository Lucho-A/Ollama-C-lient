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

#include <stdbool.h>

#define OCL_RETURN_ERROR 						-1
#define OCL_RETURN_OK 							0

#define OCL_NAME 								"libOCl"
#define OCL_MAJOR_VERSION						"0"
#define OCL_MINOR_VERSION						"0"
#define OCL_MICRO_VERSION						"1"
#define OCL_VERSION								PROGRAM_MAJOR_VERSION"."PROGRAM_MINOR_VERSION"."PROGRAM_MICRO_VERSION
#define OCL_DESCRIPTION							"C library for interacting with Ollama server"

#define OCL_DBG									printf("\nWTFFF?!?!\n");

#define OCL_OLLAMA_SERVER_ADDR					"127.0.0.1"
#define OCL_OLLAMA_SERVER_PORT					"443"

#define OCL_SOCKET_CONNECT_TIMEOUT_S			"5"
#define OCL_SOCKET_SEND_TIMEOUT_S				"5"
#define OCL_SOCKET_RECV_TIMEOUT_S				"15"

#define OCL_MODEL								""
#define OCL_KEEPALIVE							"5"
#define OCL_SYSTEM_ROLE							""
#define OCL_TEMP								"0.5"
#define OCL_MAX_HISTORY_CTX						"3"
#define OCL_MAX_TOKENS_CTX						"2048"
#define OCL_CONTEXT_FILE						""

enum ocl_errors{
	OCL_ERR_INIT_ERROR=-100,
	OCL_ERR_MALLOC_ERROR,
	OCL_ERR_REALLOC_ERROR,
	OCL_ERR_GETTING_HOST_INFO_ERROR,
	OCL_ERR_SOCKET_CREATION_ERROR,
	OCL_ERR_SOCKET_CONNECTION_ERROR,
	OCL_ERR_SOCKET_CONNECTION_TIMEOUT_ERROR,
	OCL_ERR_SSLCTX_NULL_ERROR,
	OCL_ERR_SSL_CONTEXT_ERROR,
	OCL_ERR_SSL_CERT_NOT_FOUND,
	OCL_ERR_SSL_FD_ERROR,
	OCL_ERR_SSL_CONNECT_ERROR,
	OCL_ERR_SEND_TIMEOUT_ERROR,
	OCL_ERR_SENDING_PACKETS_ERROR,
	OCL_ERR_RECV_TIMEOUT_ERROR,
	OCL_ERR_RECEIVING_PACKETS_ERROR,
	OCL_ERR_RESPONSE_MESSAGE_ERROR,
	OCL_ERR_PARTIAL_RESPONSE_RECV,
	OCL_ERR_ZEROBYTESSENT_ERROR,
	OCL_ERR_ZEROBYTESRECV_ERROR,
	OCL_ERR_MODEL_FILE_NOT_FOUND,
	OCL_ERR_CONTEXT_FILE_NOT_FOUND,
	OCL_ERR_CERT_FILE_NOT_FOUND,
	OCL_ERR_OPENING_FILE_ERROR,
	OCL_ERR_CONTEXT_FILE_CORRUPTED,
	OCL_ERR_OPENING_ROLE_FILE_ERROR,
	OCL_ERR_NO_HISTORY_CONTEXT_ERROR,
	OCL_ERR_CONTEXT_MSGS_ERROR,
	OCL_ERR_NULL_STRUCT_ERROR,
	OCL_ERR_SERVICE_UNAVAILABLE,
	OCL_ERR_GETTING_MODELS,
	OCL_ERR_LOADING_MODEL,
	OCL_ERR_UNLOADING_MODEL,
	OCL_ERR_SERVER_ADDR,
	OCL_ERR_PORT,
	OCL_ERR_KEEP_ALIVE,
	OCL_ERR_TEMP,
	OCL_ERR_MAX_HISTORY_CTX,
	OCL_ERR_MAX_TOKENS_CTX,
	OCL_ERR_SOCKET_CONNECTION_TIMEOUT_NOT_VALID,
	OCL_ERR_SOCKET_SEND_TIMEOUT_NOT_VALID,
	OCL_ERR_SOCKET_RECV_TIMEOUT_NOT_VALID,
	OCL_ERR_RESPONSE_SPEED_NOT_VALID,
	OCL_ERR_ERROR_MSG_FOUND
};

typedef struct _ocl OCl;

extern int oclSslError;
extern bool oclCanceled;

int OCl_init();
int OCl_get_instance(OCl **, const char *, const char *, const char *, const char *, const char *, const char *, const char *
		,const char *,const char *, const char *, const char *, const char *);
int OCl_free(OCl *);

int OCl_flush_context();
int OCl_load_model(OCl *, bool load);
int OCl_send_chat(OCl *, const char *, void (*)(const char *));
int OCl_check_service_status(OCl *);
int OCl_check_model_loaded(OCl *);
int OCl_import_static_context(const char *);
int OCl_import_context(const OCl *);
char * OCL_error_handling(OCl *, int);

int OCl_get_models(OCl *, char(*)[512]);
char * OCL_get_response(OCl *);
double OCL_get_response_load_duration(const OCl *);
double OCL_get_response_prompt_eval_duration(const OCl *);
double OCL_get_response_eval_duration(const OCl *);
double OCL_get_response_total_duration(const OCl *);
int OCL_get_response_prompt_eval_count(const OCl *);
int OCL_get_response_eval_count(const OCl *);
double OCL_get_response_tokens_per_sec(const OCl *);

int OCl_set_model(OCl *, const char *);
int OCl_set_role(OCl *, const char *);

#endif /* HEADERS_LIBOLLAMA_C_LIENT_H_ */
