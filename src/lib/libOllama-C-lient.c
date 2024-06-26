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
#include <poll.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE_16K						(1024*16)

typedef struct Response{
	char *fullResponse;
	char *content;
}Response;

typedef struct Message{
	char *userMessage;
	char *assistantMessage;
	bool isNew;
	struct Message *nextMessage;
}Message;

Message *rootContextMessages=NULL;
int contContextMessages=0;
SSL_CTX *sslCtx=NULL;

typedef struct _ocl{
	char *srvAddr;
	int srvPort;
	int responseSpeed;
	int socketConnectTimeout;
	int socketSendTimeout;
	int socketRecvTimeout;
	char *responseFont;
	char *model;
	char *systemRole;
	double temp;
	int maxMessageContext;
	int maxTokensContext;
	int maxTokens;
	char *contextFile;
	struct _ocl_response *ocl_resp;
}OCl;

struct _ocl_response{
	char *fullResponse;
	char *content;
	char *error;
	double loadDuration;
	double promptEvalDuration;
	double evalDuration;
	double totalDuration;
	int promptEvalCount;
	int evalCount;
	double tokensPerSec;
};


char * OCl_get_model(OCl *ocl){ return ocl->model;}

double OCL_get_response_load_duration(OCl *ocl){ return ocl->ocl_resp->loadDuration;}
double OCL_get_response_prompt_eval_duration(OCl *ocl){ return ocl->ocl_resp->promptEvalDuration;}
double OCL_get_response_eval_duration(OCl *ocl){ return ocl->ocl_resp->evalDuration;}
double OCL_get_response_total_duration(OCl *ocl){ return ocl->ocl_resp->totalDuration;}
int OCL_get_response_prompt_eval_count(OCl *ocl){ return ocl->ocl_resp->promptEvalCount;}
int OCL_get_response_eval_count(OCl *ocl){ return ocl->ocl_resp->evalCount;}
double OCL_get_response_tokens_per_sec(OCl *ocl){ return ocl->ocl_resp->tokensPerSec;}
char * OCL_get_response_error(OCl *ocl){return ocl->ocl_resp->error;}

int OCl_set_server_addr(OCl *ocl, char *serverAddr){
	if(serverAddr!=NULL && strcmp(serverAddr,"")!=0) ocl->srvAddr=serverAddr;
	return RETURN_OK;
}

static int OCl_set_server_port(OCl *ocl, char *serverPort){
	if(serverPort!=NULL && strcmp(serverPort,"")!=0){
		char *tail=NULL;
		ocl->srvPort=strtol(serverPort, &tail, 10);
		if(ocl->srvPort<1||ocl->srvPort>65535||tail[0]!=0) return OCL_ERR_PORT;
	}
	return RETURN_OK;
}

int OCl_set_model(OCl *ocl, char *model){
	if(ocl->model!=NULL) free(ocl->model);
	ocl->model=malloc(strlen(model)+1);
	memset(ocl->model,0,strlen(model)+1);
	if(model!=NULL && strcmp(model,"")!=0){
		int cont=0;
		for(int i=0;i<strlen(model);i++){
			if(model[i]!=' ') ocl->model[cont++]=model[i];
		}
	}
	return RETURN_OK;
}

int OCl_set_role(OCl *ocl, char *role){
	if(role!=NULL && strcmp(role,"")!=0){
		if(ocl->systemRole!=NULL) free(ocl->systemRole);
		ocl->systemRole=malloc(strlen(role)+1);
		memset(ocl->systemRole,0,strlen(role)+1);
		snprintf(ocl->systemRole, strlen(role)+1,"%s", role);
	}else{
		ocl->systemRole=malloc(1);
		memset(ocl->systemRole,0,1);
		ocl->systemRole[0]=0;
	}
	return RETURN_OK;
}

static int OCl_set_connect_timeout(OCl *ocl, char *connectto){
	if(connectto!=NULL && strcmp(connectto,"")!=0){
		char *tail=NULL;
		ocl->socketConnectTimeout=strtol(connectto, &tail, 10);
		if(ocl->socketConnectTimeout<1||tail[0]!=0) return OCL_ERR_SOCKET_CONNECTION_TIMEOUT_NOT_VALID;
	}
	return RETURN_OK;
}

static int OCl_set_send_timeout(OCl *ocl, char *sendto){
	if(sendto!=NULL && strcmp(sendto,"")!=0){
		char *tail=NULL;
		ocl->socketSendTimeout=strtol(sendto, &tail, 10);
		if(ocl->socketSendTimeout<1||tail[0]!=0) return OCL_ERR_SOCKET_SEND_TIMEOUT_NOT_VALID;
	}
	return RETURN_OK;
}

static int OCl_set_recv_timeout(OCl *ocl, char *recvto){
	if(recvto!=NULL && strcmp(recvto,"")!=0){
		char *tail=NULL;
		ocl->socketRecvTimeout=strtol(recvto, &tail, 10);
		if(ocl->socketRecvTimeout<1||tail[0]!=0) return OCL_ERR_SOCKET_RECV_TIMEOUT_NOT_VALID;
	}
	return RETURN_OK;
}

static int OCl_set_response_speed(OCl *ocl, char *respSpeed){
	if(respSpeed!=NULL && strcmp(respSpeed,"")!=0){
		char *tail=NULL;
		ocl->responseSpeed=strtol(respSpeed, &tail, 10);
		if(ocl->responseSpeed<1||tail[0]!=0) return OCL_ERR_RESPONSE_SPEED_NOT_VALID;
	}
	return RETURN_OK;
}

static int OCl_set_response_font(OCl *ocl, char *responseFont){
	if(responseFont!=NULL && strcmp(responseFont,"")!=0){
		ocl->responseFont=responseFont;
	}else{
		ocl->responseFont="";
	}
	return RETURN_OK;
}

static int OCl_set_max_context_msg(OCl *ocl, char *maxContextMsg){
	if(maxContextMsg!=NULL && strcmp(maxContextMsg,"")!=0){
		char *tail=NULL;
		ocl->maxMessageContext=strtol(maxContextMsg,&tail,10);
		if(ocl->maxMessageContext<0 || tail[0]!=0) return OCL_ERR_MAX_MSG_CTX;
	}
	return RETURN_OK;
}

static int OCl_set_temp(OCl *ocl, char *temp){
	if(temp!=NULL && strcmp(temp,"")!=0){
		char *tail=NULL;
		ocl->temp=strtod(temp,&tail);
		if(ocl->temp<=0.0 || tail[0]!=0) return OCL_ERR_TEMP;
	}
	return RETURN_OK;
}

static int OCl_set_max_tokens(OCl *ocl, char *maxTokens){
	if(maxTokens!=NULL && strcmp(maxTokens,"")!=0){
		char *tail=NULL;
		ocl->maxTokens=strtol(maxTokens,&tail,10);
		if(ocl->maxTokens<0 || tail[0]!=0) return OCL_ERR_MAX_TOKENS;
	}
	return RETURN_OK;
}

static int OCl_set_max_tokens_ctx(OCl *ocl, char *maxContextCtx){
	if(maxContextCtx!=NULL && strcmp(maxContextCtx,"")!=0){
		char *tail=NULL;
		ocl->maxTokensContext=strtol(maxContextCtx,&tail,10);
		if(ocl->maxTokensContext<0 || tail[0]!=0) return OCL_ERR_MAX_TOKENS_CTX;
	}
	return RETURN_OK;
}

static int OCl_set_context_file(OCl *ocl, char *contextFile){
	if(contextFile!=NULL && strcmp(contextFile,"")!=0){
		FILE *f=fopen(contextFile,"r");
		if(f==NULL) return OCL_ERR_CONTEXT_FILE_NOT_FOUND;
		fclose(f);
		ocl->contextFile=contextFile;
	}
	return RETURN_OK;
}


static int OCl_set_error(OCl *ocl, char *err){
	if(ocl->ocl_resp->error!=NULL) free(ocl->ocl_resp->error);
	ocl->ocl_resp->error=malloc(strlen(err)+1);
	memset(ocl->ocl_resp->error,0,strlen(err));
	snprintf(ocl->ocl_resp->error,strlen(err)+1,"%s",err);
	return RETURN_OK;
}

int OCl_init(){
	SSL_library_init();
	if((sslCtx=SSL_CTX_new(TLS_client_method()))==NULL) return OCL_ERR_SSL_CONTEXT_ERROR;
	SSL_CTX_set_verify(sslCtx, SSL_VERIFY_PEER, NULL);
	SSL_CTX_set_default_verify_paths(sslCtx);
	return RETURN_OK;
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
	return RETURN_OK;
}

int OCl_free(OCl *ocl){
	if(ocl!=NULL){
		if(ocl->model!=NULL){
			free(ocl->model);
			ocl->model=NULL;
		}
		if(ocl->systemRole!=NULL){
			free(ocl->systemRole);
			ocl->systemRole=NULL;
		}
		if(ocl->ocl_resp!=NULL){
			free(ocl->ocl_resp->content);
			free(ocl->ocl_resp->fullResponse);
			free(ocl->ocl_resp->error);
			free(ocl->ocl_resp);
		}
		free(ocl);
	}
	OCl_flush_context();
	return RETURN_OK;
}

int OCl_get_instance(OCl **ocl, char *serverAddr, char *serverPort, char *socketConnTo, char *socketSendTo
		,char *socketRecvTo, char *responseSpeed, char *responseFont, char *model, char *systemRole
		,char *maxContextMsg, char *temp, char *maxTokens, char *maxTokensCtx, char *contextFile){
	*ocl=malloc(sizeof(OCl));
	int retVal=0;
	OCl_set_server_addr(*ocl, OLLAMA_SERVER_ADDR);
	OCl_set_server_addr(*ocl, serverAddr);
	OCl_set_server_port(*ocl, OLLAMA_SERVER_PORT);
	if((retVal=OCl_set_server_port(*ocl, serverPort))!=RETURN_OK) return retVal;
	OCl_set_connect_timeout(*ocl, SOCKET_CONNECT_TIMEOUT_S);
	if((retVal=OCl_set_connect_timeout(*ocl, socketConnTo))!=RETURN_OK) return retVal;
	OCl_set_send_timeout(*ocl, SOCKET_SEND_TIMEOUT_MS);
	if((retVal=OCl_set_send_timeout(*ocl, socketSendTo))!=RETURN_OK) return retVal;
	OCl_set_recv_timeout(*ocl, SOCKET_RECV_TIMEOUT_MS);
	if((retVal=OCl_set_recv_timeout(*ocl, socketRecvTo))!=RETURN_OK) return retVal;
	OCl_set_response_speed(*ocl, RESPONSE_SPEED);
	if((retVal=OCl_set_response_speed(*ocl, responseSpeed))!=RETURN_OK) return retVal;
	OCl_set_response_font(*ocl, responseFont);
	(*ocl)->model=NULL;
	OCl_set_model(*ocl, model);
	(*ocl)->systemRole=NULL;
	OCl_set_role(*ocl, systemRole);
	OCl_set_max_context_msg(*ocl, MAX_HISTORY_CONTEXT);
	if((retVal=OCl_set_max_context_msg(*ocl, maxContextMsg))!=RETURN_OK) return retVal;
	OCl_set_temp(*ocl, TEMP);
	if((retVal=OCl_set_temp(*ocl, maxContextMsg))!=RETURN_OK) return retVal;
	OCl_set_max_tokens(*ocl, MAX_TOKENS);
	if((retVal=OCl_set_max_tokens(*ocl, maxContextMsg))!=RETURN_OK) return retVal;
	OCl_set_max_tokens_ctx(*ocl, NUM_CTX);
	if((retVal=OCl_set_max_tokens_ctx(*ocl, maxContextMsg))!=RETURN_OK) return retVal;
	(*ocl)->contextFile=NULL;
	if((retVal=OCl_set_context_file(*ocl,contextFile))!=RETURN_OK) return retVal;
	(*ocl)->ocl_resp=malloc(sizeof(struct _ocl_response));
	(*ocl)->ocl_resp->content=NULL;
	(*ocl)->ocl_resp->fullResponse=NULL;
	(*ocl)->ocl_resp->error=NULL;
	return RETURN_OK;
}

static void clean_ssl(SSL *ssl){
	SSL_shutdown(ssl);
	SSL_certs_clear(ssl);
	SSL_clear(ssl);
	SSL_free(ssl);
}

static int create_connection(char *srvAddr, int srvPort, int socketConnectTimeout){
	static char ollamaServerIp[INET_ADDRSTRLEN]="";
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_STREAM;
	if(getaddrinfo(srvAddr, NULL, &hints, &res)!=0) return OCL_ERR_GETTING_HOST_INFO_ERROR;
	struct sockaddr_in *ipv4=(struct sockaddr_in *)res->ai_addr;
	void *addr=&(ipv4->sin_addr);
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
	connect(socketConn, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
	fd_set rFdset, wFdset;
	struct timeval tv;
	FD_ZERO(&rFdset);
	FD_SET(socketConn, &rFdset);
	wFdset=rFdset;
	tv.tv_sec=socketConnectTimeout;
	tv.tv_usec=0;
	if(select(socketConn+1,&rFdset,&wFdset,NULL,&tv)<=0) return OCL_ERR_SOCKET_CONNECTION_TIMEOUT_ERROR;
	fcntl(socketConn, F_SETFL, socketFlags);
	return socketConn;
}

static void create_new_context_message(char *userMessage, char *assistantMessage, bool isNew, int maxHistoryContext){
	Message *newMessage=malloc(sizeof(Message));
	newMessage->userMessage=malloc(strlen(userMessage)+1);
	snprintf(newMessage->userMessage,strlen(userMessage)+1,"%s",userMessage);
	newMessage->assistantMessage=malloc(strlen(assistantMessage)+1);
	snprintf(newMessage->assistantMessage,strlen(assistantMessage)+1,"%s",assistantMessage);
	newMessage->isNew=isNew;
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


int OCl_save_message(OCl *ocl, char *userMessage, char *assistantMessage){
	if(ocl->contextFile!=NULL){
		FILE *f=fopen(ocl->contextFile,"a");
		if(f==NULL) return OCL_ERR_OPENING_FILE_ERROR;
		fprintf(f,"%s\t%s\n",userMessage,assistantMessage);
		fclose(f);
	}
	return RETURN_OK;
}

int OCl_import_context(OCl *ocl){
	if(ocl->contextFile!=NULL){
		FILE *f=fopen(ocl->contextFile,"r");
		if(f==NULL) return OCL_ERR_OPENING_FILE_ERROR;
		size_t len=0, i=0, rows=0, initPos=0;
		ssize_t chars=0;
		char *line=NULL, *userMessage=NULL,*assistantMessage=NULL;;
		while((getline(&line, &len, f))!=-1) rows++;
		if(rows>ocl->maxMessageContext) initPos=rows-ocl->maxMessageContext;
		rewind(f);
		int contRows=0;
		while((chars=getline(&line, &len, f))!=-1){
			if(contRows>=initPos){
				userMessage=malloc(chars+1);
				memset(userMessage,0,chars+1);
				for(i=0;line[i]!='\t';i++) userMessage[i]=line[i];
				int index=0;
				assistantMessage=malloc(chars+1);
				memset(assistantMessage,0,chars+1);
				for(i++;line[i]!='\n';i++,index++) assistantMessage[index]=line[i];
				create_new_context_message(userMessage, assistantMessage, FALSE, ocl->maxMessageContext);
				free(userMessage);
				free(assistantMessage);
			}
			contRows++;
		}
		free(line);
		fclose(f);
	}
	return RETURN_OK;
}

char * OCL_error_handling(int error){
	static char error_hndl[1024]="";
	int sslErr=ERR_get_error();
	switch(error){
	case OCL_ERR_MALLOC_ERROR:
		snprintf(error_hndl, 1024,"Malloc() error: %s", strerror(errno));
		break;
	case OCL_ERR_REALLOC_ERROR:
		snprintf(error_hndl, 1024,"Realloc() error: %s", strerror(errno));
		break;
	case OCL_ERR_GETTING_HOST_INFO_ERROR:
		snprintf(error_hndl, 1024,"Error getting host info: %s", strerror(errno));
		break;
	case OCL_ERR_SOCKET_CREATION_ERROR:
		snprintf(error_hndl, 1024,"Error creating socket: %s", strerror(errno));
		break;
	case OCL_ERR_SOCKET_CONNECTION_TIMEOUT_ERROR:
		snprintf(error_hndl, 1024,"Socket connection time out. ");
		break;
	case OCL_ERR_SSL_CONTEXT_ERROR:
		snprintf(error_hndl, 1024,"Error creating SSL context: %s. SSL Error: %s", strerror(errno),ERR_error_string(sslErr, NULL));
		break;
	case OCL_ERR_SSL_CERT_NOT_FOUND:
		snprintf(error_hndl, 1024,"SSL cert. not found: %s. SSL Error: %s", strerror(errno),ERR_error_string(sslErr, NULL));
		break;
	case OCL_ERR_SSL_FD_ERROR:
		snprintf(error_hndl, 1024,"SSL fd error: %s. SSL Error: %s", strerror(errno),ERR_error_string(sslErr, NULL));
		break;
	case OCL_ERR_SSL_CONNECT_ERROR:
		snprintf(error_hndl, 1024,"SSL Connection error: %s. SSL Error: %s", strerror(errno),ERR_error_string(sslErr, NULL));
		break;
	case OCL_ERR_SOCKET_SEND_TIMEOUT_ERROR:
		snprintf(error_hndl, 1024,"Sending packet time out. ");
		break;
	case OCL_ERR_SENDING_PACKETS_ERROR:
		snprintf(error_hndl, 1024,"Sending packet error. SSL Error: %s", ERR_error_string(sslErr, NULL));
		break;
	case OCL_ERR_POLLIN_ERROR:
		snprintf(error_hndl, 1024,"Pollin error. ");
		break;
	case OCL_ERR_SOCKET_RECV_TIMEOUT_ERROR:
		snprintf(error_hndl, 1024,"Receiving packet time out. ");
		break;
	case OCL_ERR_RECV_TIMEOUT_ERROR:
		snprintf(error_hndl, 1024,"Time out value not valid. ");
		break;
	case OCL_ERR_RECEIVING_PACKETS_ERROR:
		snprintf(error_hndl, 1024,"Receiving packet error. SSL Error: %s",ERR_error_string(sslErr, NULL));
		break;
	case OCL_ERR_RESPONSE_MESSAGE_ERROR:
		snprintf(error_hndl, 1024,"Error message into JSON. ");
		break;
	case OCL_ERR_ZEROBYTESRECV_ERROR:
		snprintf(error_hndl, 1024,"Zero bytes received. Try again...");
		break;
	case OCL_ERR_MODEL_FILE_NOT_FOUND:
		snprintf(error_hndl, 1024,"Model file not found. ");
		break;
	case OCL_ERR_CONTEXT_FILE_NOT_FOUND:
		snprintf(error_hndl, 1024,"Context file not found. ");
		break;
	case OCL_ERR_CERT_FILE_NOT_FOUND:
		snprintf(error_hndl, 1024,"Cert. file not found. ");
		break;
	case OCL_ERR_OPENING_FILE_ERROR:
		snprintf(error_hndl, 1024,"Error opening file: %s", strerror(errno));
		break;
	case OCL_ERR_OPENING_ROLE_FILE_ERROR:
		snprintf(error_hndl, 1024,"Error opening 'Role' file: %s", strerror(errno));
		break;
	case OCL_ERR_NO_HISTORY_CONTEXT_ERROR:
		snprintf(error_hndl, 1024,"No message to save. ");
		break;
	case OCL_ERR_UNEXPECTED_JSON_FORMAT_ERROR:
		snprintf(error_hndl, 1024,"Unexpected JSON format error. ");
		break;
	case OCL_ERR_CONTEXT_MSGS_ERROR:
		snprintf(error_hndl, 1024,"'Max. Context Message' value out-of-boundaries. ");
		break;
	case OCL_ERR_NULL_STRUCT_ERROR:
		snprintf(error_hndl, 1024,"ChatGPT structure null. ");
		break;
	case OCL_ERR_SERVICE_UNAVAILABLE:
		snprintf(error_hndl, 1024,"Service unavailable. ");
		break;
	case OCL_ERR_LOADING_MODEL:
		snprintf(error_hndl, 1024,"Error loading model. ");
		break;
	case OCL_ERR_UNLOADING_MODEL:
		snprintf(error_hndl, 1024,"Error unloading model. ");
		break;
	case OCL_ERR_SERVER_ADDR:
		snprintf(error_hndl, 1024,"Server address not valid. ");
		break;
	case OCL_ERR_PORT:
		snprintf(error_hndl, 1024,"Port not valid. ");
		break;
	case OCL_ERR_TEMP:
		snprintf(error_hndl, 1024,"Temperature value not valid. Check modfile.");
		break;
	case OCL_ERR_MAX_MSG_CTX:
		snprintf(error_hndl, 1024,"Max. message context value not valid. Check modfile.");
		break;
	case OCL_ERR_MAX_TOKENS_CTX:
		snprintf(error_hndl, 1024,"Max. tokens context value not valid. Check modfile.");
		break;
	case OCL_ERR_MAX_TOKENS:
		snprintf(error_hndl, 1024,"Max. tokens value not valid. Check modfile.");
		break;
	case OCL_ERR_SOCKET_CONNECTION_TIMEOUT_NOT_VALID:
		snprintf(error_hndl, 1024,"Connection Timeout value not valid.");
		break;
	case OCL_ERR_SOCKET_SEND_TIMEOUT_NOT_VALID:
		snprintf(error_hndl, 1024,"Send Timeout value not valid.");
		break;
	case OCL_ERR_SOCKET_RECV_TIMEOUT_NOT_VALID:
		snprintf(error_hndl, 1024,"Recv. Timeout value not valid.");
		break;
	case OCL_ERR_RESPONSE_SPEED_NOT_VALID:
		snprintf(error_hndl, 1024,"Response Speed value not valid.");
		break;

	default:
		snprintf(error_hndl, 1024,"Error not handled. ");
		break;
	}
	return error_hndl;
}

static int get_string_from_token(char *text, char *token, char *result, char endChar){
	ssize_t cont=0;
	char *message=NULL;
	message=strstr(text,token);
	if(message==NULL) return RETURN_ERROR;
	for(int i=strlen(token);(message[i-1]=='\\' || message[i]!=endChar);i++,cont++){
		if(message[i]=='\\' && message[i+1]=='\"' && message[i+2]=='}' && message[i+3]==','){
			(result)[cont]='\\';
			i+=3;
			continue;
		}
		(result)[cont]=message[i];
	}
	return RETURN_OK;
}

static int parse_input(char **stringTo, char *stringFrom){
	int cont=0, contEsc=0;
	for(int i=0;i<strlen(stringFrom);i++){
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
	for(int i=0;i<strlen(stringFrom);i++,cont++){
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
	(*stringTo)[cont]='\0';
	return RETURN_OK;
}

static int parse_output(char **stringTo, char *stringFrom){
	*stringTo=malloc(strlen(stringFrom)+1);
	memset(*stringTo,0,strlen(stringFrom)+1);
	int cont=0;
	for(int i=0;stringFrom[i]!=0;i++,cont++){
		if(stringFrom[i]=='\\'){
			switch(stringFrom[i+1]){
			case 'n':
				(*stringTo)[cont]='\n';
				break;
			case 'r':
				(*stringTo)[cont]='\r';
				break;
			case 't':
				(*stringTo)[cont]='\t';
				break;
			case '\\':
				(*stringTo)[cont]='\\';
				break;
			case '"':
				(*stringTo)[cont]='\"';
				break;
			default:
				break;
			}
			i++;
			continue;
		}
		(*stringTo)[cont]=stringFrom[i];
	}
	(*stringTo)[cont]=0;
	return RETURN_OK;
}

static void print_response(char *response, OCl *ocl){
	char *buffer=NULL;
	printf("%s",ocl->responseFont);
	parse_output(&buffer, response);
	if(ocl->responseSpeed==0){
		printf("%s",buffer);
	}else{
		for(int i=0;buffer[i]!=0 && !canceled;i++){
			usleep(ocl->responseSpeed);
			printf("%c",buffer[i]);
			fflush(stdout);
		}
	}
	free(buffer);
}

static int send_message(OCl *ocl,char *payload, char **fullResponse, char **content, bool streamed){
	int socketConn=create_connection(ocl->srvAddr, ocl->srvPort, ocl->socketConnectTimeout);
	if(socketConn<0) return OCL_ERR_SOCKET_CREATION_ERROR;
	SSL *sslConn=NULL;
	if((sslConn=SSL_new(sslCtx))==NULL){
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
	struct pollfd pfds[1];
	int numEvents=0,pollinHappened=0,bytesSent=0;
	fcntl(socketConn, F_SETFL, O_NONBLOCK);
	pfds[0].fd=socketConn;
	pfds[0].events=POLLOUT;
	numEvents=poll(pfds,1,ocl->socketSendTimeout);
	if(numEvents==0){
		close(socketConn);
		clean_ssl(sslConn);
		return OCL_ERR_SOCKET_SEND_TIMEOUT_ERROR;
	}
	pollinHappened=pfds[0].revents & POLLOUT;
	if(pollinHappened){
		int totalBytesSent=0;
		while(totalBytesSent<strlen(payload)){
			bytesSent=SSL_write(sslConn, payload + totalBytesSent, strlen(payload) - totalBytesSent);
			if(bytesSent<=0){
				close(socketConn);
				clean_ssl(sslConn);
				if(bytesSent==0) return OCL_ERR_ZEROBYTESRECV_ERROR;
				return OCL_ERR_SENDING_PACKETS_ERROR;
			}
			totalBytesSent+=bytesSent;
		}
	}else{
		close(socketConn);
		clean_ssl(sslConn);
		return OCL_ERR_POLLIN_ERROR;
	}
	ssize_t bytesReceived=0,totalBytesReceived=0;
	pfds[0].events=POLLIN;
	numEvents=poll(pfds, 1, ocl->socketRecvTimeout);
	if(numEvents==0){
		close(socketConn);
		clean_ssl(sslConn);
		return OCL_ERR_SOCKET_RECV_TIMEOUT_ERROR;
	}
	pollinHappened = pfds[0].revents & POLLIN;
	*fullResponse=malloc(1);
	(*fullResponse)[0]=0;
	if(content!=NULL){
		*content=malloc(1);
		(*content)[0]=0;
	}
	if (pollinHappened){
		do{
			char buffer[BUFFER_SIZE_16K]="";
			bytesReceived=SSL_read(sslConn,buffer, BUFFER_SIZE_16K);
			if(bytesReceived>0){
				totalBytesReceived+=bytesReceived;
				char result[BUFFER_SIZE_16K]="";
				if(streamed){
					get_string_from_token(buffer, "\"content\":\"", result, '"');
					print_response(result, ocl);
					if(content!=NULL){
						*content=realloc(*content,totalBytesReceived+1);
						strcat(*content,result);
					}
				}
				*fullResponse=realloc(*fullResponse,totalBytesReceived+1);
				strcat(*fullResponse,buffer);
				if(strstr(buffer,"\"done\":false")!=NULL || strstr(buffer,"\"done\": false")!=NULL) continue;
				if(strstr(buffer,"\"done\":true")!=NULL || strstr(buffer,"\"done\": true")!=NULL) break;
			}
			if(bytesReceived==0){
				close(socketConn);
				break;
			}
			if(bytesReceived<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) continue;
			if(bytesReceived<0 && (errno!=EAGAIN)){
				close(socketConn);
				clean_ssl(sslConn);
				return OCL_ERR_RECEIVING_PACKETS_ERROR;
			}

		}while(TRUE && !canceled);
	}
	close(socketConn);
	clean_ssl(sslConn);
	return totalBytesReceived;
}

int OCl_send_chat(OCl *ocl, char *message){
	char *messageParsed=NULL;
	parse_input(&messageParsed, message);
	char *context=malloc(1), *buf=NULL;
	context[0]=0;
	if(message[strlen(message)-1]!=';'){
		char *contextTemplate="{\"role\":\"user\",\"content\":\"%s\"},{\"role\":\"assistant\",\"content\":\"%s\"},";
		Message *temp=rootContextMessages;
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
			if(context==NULL){
				free(messageParsed);
				free(context);
				free(buf);
				return OCL_ERR_REALLOC_ERROR;
			}
			strcat(context,buf);
			temp=temp->nextMessage;
			free(buf);
		}
	}
	char *roleParsed=NULL;
	parse_input(&roleParsed, ocl->systemRole);
	ssize_t len=
			strlen(ocl->model)
			+sizeof(ocl->temp)
			+sizeof(ocl->maxTokens)
			+sizeof(ocl->maxTokensContext)
			+strlen(roleParsed)
			+strlen(context)
			+strlen(messageParsed)
			+512;
	char *body=malloc(len);
	memset(body,0,len);
	snprintf(body,len,
			"{\"model\":\"%s\","
			"\"temperature\": %f,"
			"\"max_tokens\": %d,"
			"\"num_ctx\": %d,"
			"\"stream\": true,"
			//"\"format\": \"json\","
			"\"keep_alive\": -1,"
			"\"stop\": null,"
			"\"messages\":["
			"{\"role\":\"system\",\"content\":\"%s\"},"
			"%s""{\"role\": \"user\",\"content\": \"%s\"}]}",
			ocl->model,
			ocl->temp,
			ocl->maxTokens,
			ocl->maxTokensContext,
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
	char *fullResponse=NULL, *content=NULL;
	int resp=send_message(ocl, msg, &fullResponse, &content, TRUE);
	free(msg);
	if(strstr(fullResponse,"{\"error")!=NULL){
		OCl_set_error(ocl, strstr(fullResponse,"{\"error"));
		free(messageParsed);
		free(fullResponse);
		free(content);
		return OCL_ERR_RESPONSE_MESSAGE_ERROR;
	}
	if(resp<0){
		free(messageParsed);
		free(fullResponse);
		free(content);
		return resp;
	}
	if(!canceled && resp>0){
		char result[1024]="";
		memset(result,0,1024);
		get_string_from_token(fullResponse, "\"load_duration\":", result, ',');
		ocl->ocl_resp->loadDuration=strtod(result,NULL)/1000000000.0;
		memset(result,0,1024);
		get_string_from_token(fullResponse, "\"prompt_eval_duration\":", result, ',');
		ocl->ocl_resp->promptEvalDuration=strtod(result,NULL)/1000000000.0;
		memset(result,0,1024);
		get_string_from_token(fullResponse, "\"eval_duration\":", result, '}');
		ocl->ocl_resp->evalDuration=strtod(result,NULL)/1000000000.0;
		memset(result,0,1024);
		get_string_from_token(fullResponse, "\"total_duration\":", result, ',');
		ocl->ocl_resp->totalDuration=strtod(result,NULL)/1000000000.0;
		memset(result,0,1024);
		get_string_from_token(fullResponse, "\"prompt_eval_count\":", result, ',');
		ocl->ocl_resp->promptEvalCount=strtol(result,NULL,10);
		memset(result,0,1024);
		get_string_from_token(fullResponse, "\"eval_count\":", result, ',');
		ocl->ocl_resp->evalCount=strtol(result,NULL,10);
		ocl->ocl_resp->tokensPerSec=ocl->ocl_resp->evalCount/ocl->ocl_resp->evalDuration;
		if(message[strlen(message)-1]!=';'){
			create_new_context_message(messageParsed, content, TRUE, ocl->maxMessageContext);
			OCl_save_message(ocl, messageParsed, content);
		}
	}
	free(messageParsed);
	free(fullResponse);
	free(content);
	return RETURN_OK;
}

int OCl_check_service_status(OCl *ocl){
	char msg[2048]="";
	snprintf(msg,2048,
			"GET / HTTP/1.1\r\n"
			"Host: %s\r\n\r\n",ocl->srvAddr);
	char *buffer=NULL;
	int retVal=0;
	if((retVal=send_message(ocl, msg, &buffer,NULL, FALSE))<0){
		free(buffer);
		return retVal;
	}
	if(strstr(buffer,"Ollama")==NULL){
		free(buffer);
		return OCL_ERR_SERVICE_UNAVAILABLE;
	}
	free(buffer);
	return RETURN_OK;
}


int OCl_load_model(OCl *ocl, bool load){
	char body[1024]="";
	if(load){
		snprintf(body,1024,"{\"model\": \"%s\", \"keep_alive\": -1}",ocl->model);
	}else{
		snprintf(body,1024,"{\"model\": \"%s\", \"keep_alive\": 0}",ocl->model);
	}
	char msg[2048]="";
	snprintf(msg,2048,
			"POST /api/chat HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",ocl->srvAddr,(int) strlen(body), body);
	char *buffer=NULL;
	int retVal=0;
	if((retVal=send_message(ocl, msg, &buffer, NULL, FALSE))<0) return retVal;
	if(strstr(buffer,"{\"error")!=NULL){
		OCl_set_error(ocl, strstr(buffer,"{\"error"));
		free(buffer);
		if(load) return OCL_ERR_LOADING_MODEL;
		return OCL_ERR_UNLOADING_MODEL;
	}
	if(strstr(buffer,"200 OK")!=NULL){
		free(buffer);
		return RETURN_OK;
	}
	free(buffer);
	if(load) return OCL_ERR_LOADING_MODEL;
	return OCL_ERR_UNLOADING_MODEL;
}
