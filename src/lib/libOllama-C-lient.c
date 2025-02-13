/*
 ============================================================================
 Name        : libOllama-C-lient.c
 Author      : L. (lucho-a.github.io)
 Version     : 0.0.1
 Created on	 : 2024/04/19
 Copyright   : GNU General Public License v3.0
 Description : C file
 ============================================================================
 */

#include "libOllama-C-lient.h"

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE_1K				(1024)
#define BUFFER_SIZE_2K				(1024*2)
#define BUFFER_SIZE_16K				(1024*16)
#define BUFFER_SIZE_128K			(1024*128)

typedef struct Message{
	char *userMessage;
	char *assistantMessage;
	struct Message *nextMessage;
}Message;

Message *rootContextMessages=NULL;
Message *rootStaticContextMessages=NULL;
int contContextMessages=0;
SSL_CTX *oclSslCtx=NULL;
int oclSslError=0;
bool oclCanceled=false;

typedef struct _ocl{
	char srvAddr[512];
	int srvPort;
	int socketConnectTimeout;
	int socketSendTimeout;
	int socketRecvTimeout;
	char model[512];
	int keepalive;
	char systemRole[2048];
	double temp;
	int maxHistoryCtx;
	int maxTokensCtx;
	char contextFile[512];
	struct _ocl_response *ocl_resp;
}OCl;

struct _ocl_response{
	char content[BUFFER_SIZE_128K];
	char response[BUFFER_SIZE_128K];
	char error[BUFFER_SIZE_1K];
	double loadDuration;
	double promptEvalDuration;
	double evalDuration;
	double totalDuration;
	int promptEvalCount;
	int evalCount;
	double tokensPerSec;
};

char * OCL_get_response(OCl *ocl){ return ocl->ocl_resp->content;}
double OCL_get_response_load_duration(const OCl *ocl){ return ocl->ocl_resp->loadDuration;}
double OCL_get_response_prompt_eval_duration(const OCl *ocl){ return ocl->ocl_resp->promptEvalDuration;}
double OCL_get_response_eval_duration(const OCl *ocl){ return ocl->ocl_resp->evalDuration;}
double OCL_get_response_total_duration(const OCl *ocl){ return ocl->ocl_resp->totalDuration;}
int OCL_get_response_prompt_eval_count(const OCl *ocl){ return ocl->ocl_resp->promptEvalCount;}
int OCL_get_response_eval_count(const OCl *ocl){ return ocl->ocl_resp->evalCount;}
double OCL_get_response_tokens_per_sec(const OCl *ocl){ return ocl->ocl_resp->tokensPerSec;}

int OCl_set_server_addr(OCl *ocl, const char *serverAddr){
	if(serverAddr!=NULL && strcmp(serverAddr,"")!=0) snprintf(ocl->srvAddr,512,"%s",serverAddr);
	return OCL_RETURN_OK;
}

static int OCl_set_server_port(OCl *ocl, const char *serverPort){
	if(serverPort!=NULL && strcmp(serverPort,"")!=0){
		char *tail=NULL;
		ocl->srvPort=strtol(serverPort, &tail, 10);
		if(ocl->srvPort<1||ocl->srvPort>65535|| tail[0]!=0) return OCL_ERR_PORT;
	}
	return OCL_RETURN_OK;
}

int OCl_set_model(OCl *ocl, const char *model){
	snprintf(ocl->model,512,"%s",model);
	return OCL_RETURN_OK;
}

static int OCl_set_keepalive(OCl *ocl, char const *keepalive){
	if(keepalive!=NULL && strcmp(keepalive,"")!=0){
		char *tail=NULL;
		ocl->keepalive=strtol(keepalive,&tail,10);
		if(ocl->keepalive<1 || tail[0]!=0) return OCL_ERR_KEEP_ALIVE;
	}
	return OCL_RETURN_OK;
}

int OCl_set_role(OCl *ocl, const char *role){
	snprintf(ocl->systemRole, 2048,"%s", role);
	return OCL_RETURN_OK;
}

static int OCl_set_connect_timeout(OCl *ocl, char const *connectto){
	if(connectto!=NULL && strcmp(connectto,"")!=0){
		char *tail=NULL;
		ocl->socketConnectTimeout=strtol(connectto, &tail, 10);
		if(ocl->socketConnectTimeout<1||tail[0]!=0) return OCL_ERR_SOCKET_CONNECTION_TIMEOUT_NOT_VALID;
	}
	return OCL_RETURN_OK;
}

static int OCl_set_send_timeout(OCl *ocl, char const *sendto){
	if(sendto!=NULL && strcmp(sendto,"")!=0){
		char *tail=NULL;
		ocl->socketSendTimeout=strtol(sendto, &tail, 10);
		if(ocl->socketSendTimeout<1||tail[0]!=0) return OCL_ERR_SOCKET_SEND_TIMEOUT_NOT_VALID;
	}
	return OCL_RETURN_OK;
}

static int OCl_set_recv_timeout(OCl *ocl, char const *recvto){
	if(recvto!=NULL && strcmp(recvto,"")!=0){
		char *tail=NULL;
		ocl->socketRecvTimeout=strtol(recvto, &tail, 10);
		if(ocl->socketRecvTimeout<1||tail[0]!=0) return OCL_ERR_SOCKET_RECV_TIMEOUT_NOT_VALID;
	}
	return OCL_RETURN_OK;
}

static int OCl_set_temp(OCl *ocl, char const *temp){
	if(temp!=NULL && strcmp(temp,"")!=0){
		char *tail=NULL;
		ocl->temp=strtod(temp,&tail);
		if(ocl->temp<=0.0 || tail[0]!=0) return OCL_ERR_TEMP;
	}
	return OCL_RETURN_OK;
}

static int OCl_set_max_history_ctx(OCl *ocl, char const *maxHistoryCtx){
	if(maxHistoryCtx!=NULL && strcmp(maxHistoryCtx,"")!=0){
		char *tail=NULL;
		ocl->maxHistoryCtx=strtol(maxHistoryCtx,&tail,10);
		if(ocl->maxHistoryCtx<0 || tail[0]!=0) return OCL_ERR_MAX_HISTORY_CTX;
	}
	return OCL_RETURN_OK;
}

static int OCl_set_max_tokens_ctx(OCl *ocl, char const *maxTokensCtx){
	if(maxTokensCtx!=NULL && strcmp(maxTokensCtx,"")!=0){
		char *tail=NULL;
		ocl->maxTokensCtx=strtol(maxTokensCtx,&tail,10);
		if(ocl->maxTokensCtx<0 || tail[0]!=0) return OCL_ERR_MAX_TOKENS_CTX;
	}
	return OCL_RETURN_OK;
}

static int OCl_set_context_file(OCl *ocl, const char *contextFile){
	if(contextFile!=NULL && strcmp(contextFile,"")!=0){
		FILE *f=fopen(contextFile,"r");
		if(f==NULL) return OCL_ERR_CONTEXT_FILE_NOT_FOUND;
		fclose(f);
		snprintf(ocl->contextFile,512,"%s",contextFile);
	}
	return OCL_RETURN_OK;
}

int OCl_init(){
	SSL_library_init();
	ERR_load_crypto_strings();
	if((oclSslCtx=SSL_CTX_new(TLS_client_method()))==NULL) return OCL_ERR_SSL_CONTEXT_ERROR;
	SSL_CTX_set_verify(oclSslCtx, SSL_VERIFY_PEER, NULL);
	SSL_CTX_set_default_verify_paths(oclSslCtx);
	oclCanceled=false;
	oclSslError=0;
	return OCL_RETURN_OK;
}

int OCl_flush_context(void){
	while(rootContextMessages!=NULL){
		Message *temp=rootContextMessages;
		rootContextMessages=temp->nextMessage;
		if(temp->userMessage!=NULL) free(temp->userMessage);
		if(temp->assistantMessage!=NULL) free(temp->assistantMessage);
		free(temp);
	}
	contContextMessages=0;
	return OCL_RETURN_OK;
}

int OCl_free(OCl *ocl){
	if(ocl->ocl_resp!=NULL) free(ocl->ocl_resp);
	if(ocl!=NULL) free(ocl);
	OCl_flush_context();
	return OCL_RETURN_OK;
}

int OCl_get_instance(OCl **ocl, const char *serverAddr, const char *serverPort, const char *socketConnTo, const char *socketSendTo
		,const char *socketRecvTo, const char *model, const char *keepAlive, const char *systemRole
		,const char *maxContextMsg, const char *temp, const char *maxTokensCtx, const char *contextFile){
	*ocl=malloc(sizeof(OCl));
	int retVal=0;
	OCl_set_server_addr(*ocl, OCL_OLLAMA_SERVER_ADDR);
	OCl_set_server_addr(*ocl, serverAddr);
	OCl_set_server_port(*ocl, OCL_OLLAMA_SERVER_PORT);
	if((retVal=OCl_set_server_port(*ocl, serverPort))!=OCL_RETURN_OK) return retVal;
	OCl_set_connect_timeout(*ocl, OCL_SOCKET_CONNECT_TIMEOUT_S);
	if((retVal=OCl_set_connect_timeout(*ocl, socketConnTo))!=OCL_RETURN_OK) return retVal;
	OCl_set_send_timeout(*ocl, OCL_SOCKET_SEND_TIMEOUT_S);
	if((retVal=OCl_set_send_timeout(*ocl, socketSendTo))!=OCL_RETURN_OK) return retVal;
	OCl_set_recv_timeout(*ocl, OCL_SOCKET_RECV_TIMEOUT_S);
	if((retVal=OCl_set_recv_timeout(*ocl, socketRecvTo))!=OCL_RETURN_OK) return retVal;
	OCl_set_model(*ocl, OCL_MODEL);
	OCl_set_model(*ocl, model);
	OCl_set_keepalive(*ocl, OCL_KEEPALIVE);
	if((retVal=OCl_set_keepalive(*ocl, keepAlive))!=OCL_RETURN_OK) return retVal;
	OCl_set_role(*ocl, OCL_SYSTEM_ROLE);
	OCl_set_role(*ocl, systemRole);
	OCl_set_temp(*ocl, OCL_TEMP);
	if((retVal=OCl_set_temp(*ocl, temp))!=OCL_RETURN_OK) return retVal;
	OCl_set_max_history_ctx(*ocl, OCL_MAX_HISTORY_CTX);
	if((retVal=OCl_set_max_history_ctx(*ocl, maxContextMsg))!=OCL_RETURN_OK) return retVal;
	OCl_set_max_tokens_ctx(*ocl, OCL_MAX_TOKENS_CTX);
	if((retVal=OCl_set_max_tokens_ctx(*ocl, maxTokensCtx))!=OCL_RETURN_OK) return retVal;
	if((retVal=OCl_set_context_file(*ocl,OCL_CONTEXT_FILE))!=OCL_RETURN_OK) return retVal;
	if((retVal=OCl_set_context_file(*ocl,contextFile))!=OCL_RETURN_OK) return retVal;
	(*ocl)->ocl_resp=malloc(sizeof(struct _ocl_response));
	memset((*ocl)->ocl_resp->content,0,BUFFER_SIZE_128K);
	memset((*ocl)->ocl_resp->response,0,BUFFER_SIZE_128K);
	memset((*ocl)->ocl_resp->error,0,BUFFER_SIZE_1K);
	(*ocl)->ocl_resp->loadDuration=0.0;
	(*ocl)->ocl_resp->promptEvalDuration=0.0;
	(*ocl)->ocl_resp->evalDuration=0.0;
	(*ocl)->ocl_resp->totalDuration=0.0;
	(*ocl)->ocl_resp->promptEvalCount=0;
	(*ocl)->ocl_resp->evalCount=0;
	(*ocl)->ocl_resp->tokensPerSec=0.0;
	return OCL_RETURN_OK;
}

static void clean_ssl(SSL *ssl){
	SSL_shutdown(ssl);
	SSL_certs_clear(ssl);
	SSL_clear(ssl);
	SSL_free(ssl);
}

static void create_new_static_context_message(char *userMessage, char *assistantMessage){
	Message *newMessage=malloc(sizeof(Message));
	newMessage->userMessage=malloc(strlen(userMessage)+1);
	snprintf(newMessage->userMessage,strlen(userMessage)+1,"%s",userMessage);
	newMessage->assistantMessage=malloc(strlen(assistantMessage)+1);
	snprintf(newMessage->assistantMessage,strlen(assistantMessage)+1,"%s",assistantMessage);
	Message *temp=rootStaticContextMessages;
	if(temp!=NULL){
		while(temp->nextMessage!=NULL) temp=temp->nextMessage;
		temp->nextMessage=newMessage;
	}else{
		rootStaticContextMessages=newMessage;
	}
	newMessage->nextMessage=NULL;
}

static void create_new_context_message(char *userMessage, char *assistantMessage, int maxHistoryContext){
	Message *newMessage=malloc(sizeof(Message));
	newMessage->userMessage=malloc(strlen(userMessage)+1);
	snprintf(newMessage->userMessage,strlen(userMessage)+1,"%s",userMessage);
	newMessage->assistantMessage=malloc(strlen(assistantMessage)+1);
	snprintf(newMessage->assistantMessage,strlen(assistantMessage)+1,"%s",assistantMessage);
	if(rootContextMessages!=NULL){
		if(contContextMessages>=maxHistoryContext){
			Message *temp=rootContextMessages->nextMessage;
			if(rootContextMessages->userMessage!=NULL) free(rootContextMessages->userMessage);
			if(rootContextMessages->assistantMessage!=NULL) free(rootContextMessages->assistantMessage);
			free(rootContextMessages);
			rootContextMessages=temp;
		}
		Message *temp=rootContextMessages;
		if(temp!=NULL){
			while(temp->nextMessage!=NULL) temp=temp->nextMessage;
			temp->nextMessage=newMessage;
		}else{
			rootContextMessages=newMessage;
		}
	}else{
		rootContextMessages=newMessage;
	}
	newMessage->nextMessage=NULL;
	contContextMessages++;
}

int OCl_save_message(const OCl *ocl, char *userMessage, char *assistantMessage){
	FILE *f=fopen(ocl->contextFile,"a");
	if(f==NULL) return OCL_ERR_OPENING_FILE_ERROR;
	fprintf(f,"%s\t%s\n",userMessage,assistantMessage);
	fclose(f);
	return OCL_RETURN_OK;
}

int OCl_import_static_context(const char *filename){
	FILE *f=fopen(filename,"r");
	if(f==NULL) return OCL_ERR_OPENING_FILE_ERROR;
	size_t len=0, i=0;
	ssize_t chars=0;
	char *line=NULL, *userMessage=NULL,*assistantMessage=NULL;
	while((chars=getline(&line, &len, f))!=-1){
		if(strstr(line,"\t")==NULL){
			free(line);
			fclose(f);
			return OCL_ERR_CONTEXT_FILE_CORRUPTED;
		}
		userMessage=malloc(chars+1);
		memset(userMessage,0,chars+1);
		for(i=0;line[i]!='\t' && i<strlen(line) ;i++) userMessage[i]=line[i];
		int index=0;
		assistantMessage=malloc(chars+1);
		memset(assistantMessage,0,chars+1);
		for(i++;line[i]!='\n';i++,index++) assistantMessage[index]=line[i];
		create_new_static_context_message(userMessage, assistantMessage);
		free(userMessage);
		free(assistantMessage);
	}
	free(line);
	fclose(f);
	return OCL_RETURN_OK;
}

int OCl_import_context(const OCl *ocl){
	FILE *f=fopen(ocl->contextFile,"r");
	if(f==NULL) return OCL_ERR_OPENING_FILE_ERROR;
	size_t len=0, i=0;
	int rows=0, initPos=0;
	ssize_t chars=0;
	char *line=NULL, *userMessage=NULL,*assistantMessage=NULL;
	while((getline(&line, &len, f))!=-1) rows++;
	if(rows>ocl->maxHistoryCtx) initPos=rows-ocl->maxHistoryCtx;
	rewind(f);
	int contRows=0;
	while((chars=getline(&line, &len, f))!=-1){
		if(strstr(line,"\t")==NULL){
			free(line);
			fclose(f);
			return OCL_ERR_CONTEXT_FILE_CORRUPTED;
		}
		if(contRows>=initPos){
			userMessage=malloc(chars+1);
			memset(userMessage,0,chars+1);
			for(i=0;line[i]!='\t' && i<strlen(line) ;i++) userMessage[i]=line[i];
			int index=0;
			assistantMessage=malloc(chars+1);
			memset(assistantMessage,0,chars+1);
			for(i++;line[i]!='\n';i++,index++) assistantMessage[index]=line[i];
			create_new_context_message(userMessage, assistantMessage,ocl->maxHistoryCtx);
			free(userMessage);
			free(assistantMessage);
		}
		contRows++;
	}
	free(line);
	fclose(f);
	return OCL_RETURN_OK;
}

char * OCL_error_handling(OCl *ocl, int error){
	static char error_hndl[BUFFER_SIZE_2K]="";
	switch(error){
	case OCL_ERR_MALLOC_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Malloc() error: %s", strerror(errno));
		break;
	case OCL_ERR_REALLOC_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Realloc() error: %s", strerror(errno));
		break;
	case OCL_ERR_GETTING_HOST_INFO_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error getting host info: %s", strerror(errno));
		break;
	case OCL_ERR_SOCKET_CREATION_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error creating socket: %s", strerror(errno));
		break;
	case OCL_ERR_SOCKET_CONNECTION_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error connecting socket: %s", strerror(errno));
		break;
	case OCL_ERR_SOCKET_CONNECTION_TIMEOUT_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Socket connection time out. ");
		break;
	case OCL_ERR_SSLCTX_NULL_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: SSL context null: %s (Did you call OCL_init()?). SSL Error: %s", strerror(errno),ERR_error_string(oclSslError, NULL));
		break;
	case OCL_ERR_SSL_CONTEXT_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error creating SSL context: %s. SSL Error: %s", strerror(errno),ERR_error_string(oclSslError, NULL));
		break;
	case OCL_ERR_SSL_CERT_NOT_FOUND:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: SSL cert. not found: %s. SSL Error: %s", strerror(errno),ERR_error_string(oclSslError, NULL));
		break;
	case OCL_ERR_SSL_FD_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: SSL fd error: %s. SSL Error: %s", strerror(errno),ERR_error_string(oclSslError, NULL));
		break;
	case OCL_ERR_SSL_CONNECT_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: SSL Connection error: %s. SSL Error: %s", strerror(errno),ERR_error_string(oclSslError, NULL));
		break;
	case OCL_ERR_SEND_TIMEOUT_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Sending packet time out ");
		break;
	case OCL_ERR_SENDING_PACKETS_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Sending packet error. SSL Error: %s", ERR_error_string(oclSslError, NULL));
		break;
	case OCL_ERR_RECV_TIMEOUT_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Receiving packet time out: %s. SSL Error: %s", strerror(errno),ERR_error_string(oclSslError, NULL));
		break;
	case OCL_ERR_RECEIVING_PACKETS_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Receiving packet error: %s. SSL Error: %s", strerror(errno),ERR_error_string(oclSslError, NULL));
		break;
	case OCL_ERR_RESPONSE_MESSAGE_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error message into JSON. ");
		break;
	case OCL_ERR_PARTIAL_RESPONSE_RECV:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Partial response received. ");
		break;
	case OCL_ERR_ZEROBYTESSENT_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Zero bytes sent. Try again...");
		break;
	case OCL_ERR_ZEROBYTESRECV_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Zero bytes received. Try again...");
		break;
	case OCL_ERR_MODEL_FILE_NOT_FOUND:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Model file not found ");
		break;
	case OCL_ERR_CONTEXT_FILE_NOT_FOUND:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Context file not found ");
		break;
	case OCL_ERR_CONTEXT_FILE_CORRUPTED:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Context file corrupted ");
		break;
	case OCL_ERR_CERT_FILE_NOT_FOUND:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Cert. file not found ");
		break;
	case OCL_ERR_OPENING_FILE_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error opening file: %s", strerror(errno));
		break;
	case OCL_ERR_OPENING_ROLE_FILE_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error opening 'Role' file: %s", strerror(errno));
		break;
	case OCL_ERR_NO_HISTORY_CONTEXT_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: No message to save ");
		break;
	case OCL_ERR_CONTEXT_MSGS_ERROR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: 'Max. Context Message' value out-of-boundaries. ");
		break;
	case OCL_ERR_SERVICE_UNAVAILABLE:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Service unavailable ");
		break;
	case OCL_ERR_GETTING_MODELS:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error getting models ");
		break;
	case OCL_ERR_LOADING_MODEL:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error loading model. %s", ocl->ocl_resp->error);
		break;
	case OCL_ERR_UNLOADING_MODEL:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error unloading model ");
		break;
	case OCL_ERR_SERVER_ADDR:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Server address not valid ");
		break;
	case OCL_ERR_PORT:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Port not valid ");
		break;
	case OCL_ERR_KEEP_ALIVE:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Keep alive value not valid. Check modfile");
		break;
	case OCL_ERR_TEMP:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Temperature value not valid. Check modfile");
		break;
	case OCL_ERR_MAX_HISTORY_CTX:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Max. message context value not valid. Check modfile");
		break;
	case OCL_ERR_MAX_TOKENS_CTX:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Max. tokens context value not valid. Check modfile");
		break;
	case OCL_ERR_SOCKET_CONNECTION_TIMEOUT_NOT_VALID:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Connection Timeout value not valid");
		break;
	case OCL_ERR_SOCKET_SEND_TIMEOUT_NOT_VALID:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Send Timeout value not valid");
		break;
	case OCL_ERR_SOCKET_RECV_TIMEOUT_NOT_VALID:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Recv. Timeout value not valid");
		break;
	case OCL_ERR_RESPONSE_SPEED_NOT_VALID:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Response Speed value not valid");
		break;
	case OCL_ERR_ERROR_MSG_FOUND:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: %s", ocl->ocl_resp->error);
		break;
	default:
		snprintf(error_hndl, BUFFER_SIZE_2K,"OCl ERROR: Error not handled. Errno: %s", strerror(errno));
		break;
	}
	return error_hndl;
}

static bool get_string_from_token(char const *text, char const *token, char *result, char endChar){
	char const *content=strstr(text,token);
	if(content!=NULL){
		size_t i=0, len=strlen(token);
		for(i=len+1;(content[i-1]=='\\' || content[i]!=endChar);i++) result[i-len-1]=content[i];
		result[i-len-1]=0;
		return true;
	}
	return false;
}

static int parse_input(char **stringTo, char const *stringFrom){
	if(stringFrom==NULL) return OCL_RETURN_OK;
	int cont=0, contEsc=0;
	for(size_t i=0;i<strlen(stringFrom);i++){
		switch(stringFrom[i]){
		case '\"':
		case '\n':
		case '\t':
		case '\r':
		case '\\':
			contEsc++;
			break;
		default:
			break;
		}
	}
	*stringTo=malloc(strlen(stringFrom)+contEsc+1);
	memset(*stringTo,0,strlen(stringFrom)+contEsc+1);
	for(size_t i=0;i<strlen(stringFrom);i++,cont++){
		switch(stringFrom[i]){
		case '\"':
			(*stringTo)[cont]='\\';
			(*stringTo)[++cont]='\"';
			break;
		case '\n':
			(*stringTo)[cont]='\\';
			(*stringTo)[++cont]='n';
			break;
		case '\t':
			(*stringTo)[cont]='\\';
			(*stringTo)[++cont]='t';
			break;
		case '\r':
			(*stringTo)[cont]='\\';
			(*stringTo)[++cont]='r';
			break;
		case '\\':
			(*stringTo)[cont]='\\';
			(*stringTo)[++cont]='\\';
			break;
		default:
			(*stringTo)[cont]=stringFrom[i];
			break;
		}
	}
	(*stringTo)[cont]=0;
	return OCL_RETURN_OK;
}


static int create_connection(const char *srvAddr, int srvPort, int socketConnectTimeout){
	static char ollamaServerIp[INET_ADDRSTRLEN]="";
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_STREAM;
	if(getaddrinfo(srvAddr, NULL, &hints, &res)!=0) return OCL_ERR_GETTING_HOST_INFO_ERROR;
	struct sockaddr_in const *ipv4=(struct sockaddr_in *)res->ai_addr;
	void const *addr=&(ipv4->sin_addr);
	inet_ntop(res->ai_family, addr, ollamaServerIp, sizeof(ollamaServerIp));
	freeaddrinfo(res);
	int socketConn=0;
	struct sockaddr_in serverAddress;
	serverAddress.sin_family=AF_INET;
	serverAddress.sin_port=htons(srvPort);
	serverAddress.sin_addr.s_addr=inet_addr(ollamaServerIp);
	if((socketConn=socket(AF_INET, SOCK_STREAM, 0))<0) return OCL_ERR_SOCKET_CREATION_ERROR;
	int socketFlags=fcntl(socketConn, F_GETFL, 0);
	fcntl(socketConn, F_SETFL, socketFlags | O_NONBLOCK);
	struct timeval tvConnectionTo;
	int retVal=connect(socketConn, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
	if(retVal<0 && errno!=EINPROGRESS) return OCL_ERR_SOCKET_CONNECTION_ERROR;
	fd_set rFdset, wFdset;
	FD_ZERO(&rFdset);
	FD_SET(socketConn, &rFdset);
	wFdset=rFdset;
	tvConnectionTo.tv_sec=socketConnectTimeout;
	tvConnectionTo.tv_usec=0;
	if((retVal=select(socketConn+1,&rFdset,&wFdset,NULL,&tvConnectionTo))<=0){
		if(retVal==0) return OCL_ERR_SOCKET_CONNECTION_TIMEOUT_ERROR;
		return retVal;
	}
	fcntl(socketConn, F_SETFL, socketFlags & ~O_NONBLOCK);
	return socketConn;
}

static int send_message(OCl *ocl, char const *payload, void (*callback)(const char *)){
	oclSslError=0;
	int socketConn=create_connection(ocl->srvAddr, ocl->srvPort, ocl->socketConnectTimeout);
	if(socketConn<=0) return socketConn;
	if(oclSslCtx==NULL) return OCL_ERR_SSLCTX_NULL_ERROR;
	SSL *sslConn=NULL;
	if((sslConn=SSL_new(oclSslCtx))==NULL){
		clean_ssl(sslConn);
		return OCL_ERR_SSL_CONTEXT_ERROR;
	}
	if(!SSL_set_fd(sslConn, socketConn)){
		clean_ssl(sslConn);
		return OCL_ERR_SSL_FD_ERROR;
	}
	SSL_set_connect_state(sslConn);
	SSL_set_tlsext_host_name(sslConn, ocl->srvAddr);
	if(!SSL_connect(sslConn)){
		clean_ssl(sslConn);
		return OCL_ERR_SSL_CONNECT_ERROR;
	}
	fd_set rFdset, wFdset;
	size_t totalBytesSent=0;
	struct timeval tvSendTo;
	tvSendTo.tv_sec=ocl->socketSendTimeout;
	tvSendTo.tv_usec=0;
	int retVal=0;
	while(totalBytesSent<strlen(payload)){
		FD_ZERO(&wFdset);
		FD_SET(socketConn, &wFdset);
		if((retVal=select(socketConn+1,NULL,&wFdset,NULL,&tvSendTo))<=0){
			oclSslError=SSL_get_error(sslConn, retVal);
			if(retVal<0) return OCL_ERR_SENDING_PACKETS_ERROR;
			return OCL_ERR_SEND_TIMEOUT_ERROR;
		}
		totalBytesSent+=SSL_write(sslConn, payload + totalBytesSent, strlen(payload) - totalBytesSent);
	}
	ssize_t bytesReceived=0,totalBytesReceived=0;
	struct timeval tvRecvTo;
	tvRecvTo.tv_sec=ocl->socketRecvTimeout;
	tvRecvTo.tv_usec=0;
	memset(ocl->ocl_resp->content,0,BUFFER_SIZE_128K);
	memset(ocl->ocl_resp->response,0,BUFFER_SIZE_128K);
	memset(ocl->ocl_resp->error,0,BUFFER_SIZE_1K);
	do{
		FD_ZERO(&rFdset);
		FD_SET(socketConn, &rFdset);
		if((retVal=select(socketConn+1,&rFdset,NULL,NULL,&tvRecvTo))<=0){
			oclSslError=SSL_get_error(sslConn, retVal);
			switch(oclSslError){
			case 5: //SSL_ERROR_SYSCALL?? PROXY/VPN issues??
				continue;
			default:
				close(socketConn);
				clean_ssl(sslConn);
				if(retVal==0) return OCL_ERR_RECV_TIMEOUT_ERROR;
				return OCL_ERR_RECEIVING_PACKETS_ERROR;
			}
		}
		char buffer[BUFFER_SIZE_16K]="";
		bytesReceived=SSL_read(sslConn,buffer, BUFFER_SIZE_16K);
		if(bytesReceived<0){
			oclSslError=SSL_get_error(sslConn, bytesReceived);
			return OCL_ERR_RECEIVING_PACKETS_ERROR;
		}
		if(bytesReceived==0) break;
		if(bytesReceived>0){
			totalBytesReceived+=bytesReceived;
			char token[128]="";
			if(get_string_from_token(buffer, "\"content\":", token, '"')){
				char const *b=strstr(token,"\\\"},");
				if(b){
					int idx=0;
					for(size_t i=0;i<strlen(token);i++,idx++){
						if(b==&token[i]){
							token[idx]='\\';
							token[idx+1]=0;
							i+=3;
						}else{
							token[idx]=token[i];
						}
					}
				}
				if(callback!=NULL) callback(token);
				strcat(ocl->ocl_resp->content,token);
			}
			if(strstr(buffer,"\"done\":false")!=NULL || strstr(buffer,"\"done\": false")!=NULL) continue;
			if(strstr(buffer,"\"done\":true")!=NULL || strstr(buffer,"\"done\": true")!=NULL
					|| strstr(buffer,"{\"models\":[{\"")!=NULL){
				strcat(ocl->ocl_resp->response,buffer);
				break;
			}
			const char *err=NULL;
			if((err=strstr(buffer,"{\"error\":\""))!=NULL){
				strcat(ocl->ocl_resp->error,err);
				close(socketConn);
				clean_ssl(sslConn);
				return OCL_ERR_ERROR_MSG_FOUND;
			}
			if(strstr(buffer,"HTTP/1.1 503 Service Unavailable")!=NULL){
				close(socketConn);
				clean_ssl(sslConn);
				return OCL_ERR_SERVICE_UNAVAILABLE;
			}
		}
		if(!SSL_pending(sslConn)) break;
	}while(true && !oclCanceled);
	close(socketConn);
	clean_ssl(sslConn);
	return totalBytesReceived;
}

int OCl_send_chat(OCl *ocl, const char *message, void (*callback)(const char *)){
	char *messageParsed=NULL;
	parse_input(&messageParsed, message);
	char *context=malloc(1), *buf=NULL;
	context[0]=0;
	char const *contextTemplate="{\"role\":\"user\",\"content\":\"%s\"},{\"role\":\"assistant\",\"content\":\"%s\"},";
	Message *temp=rootStaticContextMessages;
	ssize_t len=0;
	while(temp!=NULL){
		len=strlen(contextTemplate)+strlen(temp->userMessage)+strlen(temp->assistantMessage);
		buf=malloc(len);
		if(buf==NULL){
			free(messageParsed);
			free(context);
			return OCL_ERR_MALLOC_ERROR;
		}
		memset(buf,0,len);
		snprintf(buf,len,contextTemplate,temp->userMessage,temp->assistantMessage);
		context=realloc(context, strlen(context)+strlen(buf)+1);
		strcat(context,buf);
		temp=temp->nextMessage;
		free(buf);
	}

	if(message[strlen(message)-1]!=';'){
		temp=rootContextMessages;
		while(temp!=NULL){
			len=strlen(contextTemplate)+strlen(temp->userMessage)+strlen(temp->assistantMessage);
			buf=malloc(len);
			if(buf==NULL){
				free(messageParsed);
				free(context);
				return OCL_ERR_MALLOC_ERROR;
			}
			memset(buf,0,len);
			snprintf(buf,len,contextTemplate,temp->userMessage,temp->assistantMessage);
			context=realloc(context, strlen(context)+strlen(buf)+1);
			strcat(context,buf);
			temp=temp->nextMessage;
			free(buf);
		}
	}
	char *roleParsed=NULL;
	parse_input(&roleParsed, ocl->systemRole);
	len=
			strlen(ocl->model)
			+sizeof(ocl->temp)
			+sizeof(ocl->maxTokensCtx)
			+strlen(roleParsed)
			+strlen(context)
			+strlen(messageParsed)
			+512;
	char *body=malloc(len);
	memset(body,0,len);
	snprintf(body,len,
			"{\"model\":\"%s\","
			"\"temperature\": %f,"
			"\"num_ctx\": %d,"
			"\"stream\": %s,"
			"\"keep_alive\": -1,"
			"\"stop\": null,"
			"\"messages\":["
			"{\"role\":\"system\",\"content\":\"%s\"},"
			"%s""{\"role\": \"user\",\"content\": \"%s\"}]}",
			ocl->model,
			ocl->temp,
			ocl->maxTokensCtx,"true",
			roleParsed,context,
			messageParsed);
	free(context);
	free(roleParsed);
	len=strlen(ocl->srvAddr)+sizeof(ocl->srvPort)+sizeof((int) strlen(body))+strlen(body)+512;
	char *msg=malloc(len);
	memset(msg,0,len);
	snprintf(msg,len,
			"POST /api/chat HTTP/1.1\r\n"
			"Host: %s\r\n"
			"User-agent: Ollama-C-lient/0.0.1 (Linux; x64)\r\n"
			"Accept: */*\r\n"
			"Content-Type: application/json; charset=utf-8\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",ocl->srvAddr,(int) strlen(body), body);
	free(body);
	int retVal=send_message(ocl, msg, callback);
	free(msg);
	if(retVal<0){
		free(messageParsed);
		return retVal;
	}
	if(retVal==0){
		free(messageParsed);
		return OCL_ERR_RECV_TIMEOUT_ERROR;
	}
	if((strstr(ocl->ocl_resp->response,"\"done\":true")==NULL || strstr(ocl->ocl_resp->response,"\"done\": true")!=NULL) && !oclCanceled){
		free(messageParsed);
		return OCL_ERR_PARTIAL_RESPONSE_RECV;
	}
	if(!oclCanceled && retVal>0){
		char result[128]="";
		if(get_string_from_token(ocl->ocl_resp->response, "\"load_duration\":", result, ',')) ocl->ocl_resp->loadDuration=strtod(result,NULL)/1000000000.0;
		if(get_string_from_token(ocl->ocl_resp->response, "\"prompt_eval_duration\":", result, ',')) ocl->ocl_resp->promptEvalDuration=strtod(result,NULL)/1000000000.0;
		if(get_string_from_token(ocl->ocl_resp->response, "\"eval_duration\":", result, '}')) ocl->ocl_resp->evalDuration=strtod(result,NULL)/1000000000.0;
		if(get_string_from_token(ocl->ocl_resp->response, "\"total_duration\":", result, ',')) ocl->ocl_resp->totalDuration=strtod(result,NULL)/1000000000.0;
		if(get_string_from_token(ocl->ocl_resp->response, "\"prompt_eval_count\":", result, ',')) ocl->ocl_resp->promptEvalCount=strtol(result,NULL,10);
		if(get_string_from_token(ocl->ocl_resp->response, "\"eval_count\":", result, ',')) ocl->ocl_resp->evalCount=strtol(result,NULL,10);
		ocl->ocl_resp->tokensPerSec=ocl->ocl_resp->evalCount/ocl->ocl_resp->evalDuration;
		if(message[strlen(message)-1]!=';'){
			create_new_context_message(messageParsed, ocl->ocl_resp->content, ocl->maxHistoryCtx);
			if(ocl->maxHistoryCtx>0) OCl_save_message(ocl, messageParsed, ocl->ocl_resp->content);
		}
	}
	free(messageParsed);
	return OCL_RETURN_OK;
}

int OCl_check_service_status(OCl *ocl){
	char msg[2048]="";
	snprintf(msg,2048,
			"GET / HTTP/1.1\r\n"
			"Host: %s\r\n\r\n",ocl->srvAddr);
	int retVal=0;
	if((retVal=send_message(ocl, msg, NULL))<=0) return retVal;
	return OCL_RETURN_OK;
}

int OCl_check_model_loaded(OCl *ocl){
	char msg[2048]="";
	snprintf(msg,2048,
			"GET /api/ps HTTP/1.1\r\n"
			"Host: %s\r\n\r\n",ocl->srvAddr);
	int retVal=0;
	if((retVal=send_message(ocl, msg, NULL))<=0) return retVal;
	if(strstr(ocl->ocl_resp->response,ocl->model)==NULL) return false;
	return true;
}

int OCl_load_model(OCl *ocl, bool load){
	char body[BUFFER_SIZE_1K]="";
	if(load){
		snprintf(body,BUFFER_SIZE_1K,"{\"model\": \"%s\", \"keep_alive\": %d}",ocl->model,ocl->keepalive);
	}else{
		snprintf(body,BUFFER_SIZE_1K,"{\"model\": \"%s\", \"keep_alive\": 0}",ocl->model);
	}
	char msg[2048]="";
	snprintf(msg,2048,
			"POST /api/chat HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",ocl->srvAddr,(int) strlen(body), body);
	int retVal=0;
	if((retVal=send_message(ocl, msg, NULL))<=0) return retVal;
	if(strstr(ocl->ocl_resp->response,"200 OK")!=NULL) return OCL_RETURN_OK;
	if(load) return OCL_ERR_LOADING_MODEL;
	return OCL_ERR_UNLOADING_MODEL;
}

int OCl_get_models(OCl *ocl, char(*models)[512]){
	char body[BUFFER_SIZE_1K]="", msg[2048]="";
	snprintf(msg,2048,
			"GET /api/tags HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",ocl->srvAddr,(int) strlen(body), body);
	int retVal=0;
	if((retVal=send_message(ocl, msg, NULL))<=0) return retVal;
	char *model=strstr(ocl->ocl_resp->response,"\"model\":\"");
	int contModel=0;
	while(model!=NULL){
		size_t cont=0, len=strlen("\"model\":\"");
		for(size_t i=len;(model[i-1]=='\\' || model[i]!='"');i++, cont++) models[contModel][cont]=model[i];
		models[contModel][cont]=0;
		contModel++;
		model[0]=' ';
		model=strstr(model,"\"model\":\"");
	}
	return contModel;
}
