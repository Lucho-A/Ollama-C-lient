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

typedef struct _ocl{
	char *srvAddr;
	int srvPort;
	int responseSpeed;
	int socketConnectTimeout;
	int socketSendTimeout;
	int socketRecvTimeout;
	char *model;
	char *systemRole;
	double temp;
	int maxMessageContext;
	int maxTokensContext;
	int maxTokens;
	char *contextFile;
	bool showResponseInfo;
}OCl;

int OCl_set_server_addr(OCl *ocl, char *param){
	if(param==NULL) return ERR_SERVER_ADDR;
	ocl->srvAddr=param;
	return RETURN_OK;
}

int OCl_set_server_port(OCl *ocl, char *param){
	if(param==NULL) return ERR_PORT;
	char *tail=NULL;
	int value=0;
	value=strtol((char*)param, &tail, 10);
	if(value<1||value>65535||strcmp(tail,"")!=0) return ERR_PORT;
	ocl->srvPort=value;
	return RETURN_OK;
}

int OCl_set_contextfile(OCl *ocl, char *param){
	if(param==NULL) return ERR_CONTEXT_FILE_NOT_FOUND;
	FILE *f=fopen(param,"r");
	if(f==NULL) return ERR_CONTEXT_FILE_NOT_FOUND;
	fclose(f);
	ocl->contextFile=param;
	return RETURN_OK;
}

int OCl_set_show_resp_info(OCl *ocl, bool param){
	ocl->showResponseInfo=param;
	return RETURN_OK;
}

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

void OCl_init_colors(bool uncolored){
	if(uncolored){
		Colors.yellow=Colors.h_green=Colors.h_red=Colors.h_cyan=Colors.h_white=Colors.def="";
		return;
	}
	Colors.yellow="\e[0;33m";
	Colors.h_green="\e[0;92m";
	Colors.h_red="\e[0;91m";
	Colors.h_cyan="\e[0;96m";
	Colors.h_white="\e[0;97m";
	Colors.def="\e[0m";
}

int OCl_init(){
	SSL_library_init();
	if((sslCtx=SSL_CTX_new(TLS_client_method()))==NULL) return ERR_SSL_CONTEXT_ERROR;
	SSL_CTX_set_verify(sslCtx, SSL_VERIFY_PEER, NULL);
	SSL_CTX_set_default_verify_paths(sslCtx);
	return RETURN_OK;
}

OCl * OCl_get_instance(char *modelfile){
	SSL_library_init();
	OCl *ocl=malloc(sizeof(OCl));
	ocl->srvAddr=OLLAMA_SERVER_ADDR;
	ocl->srvPort=OLLAMA_SERVER_PORT;
	ocl->socketConnectTimeout=SOCKET_CONNECT_TIMEOUT_S;
	ocl->socketSendTimeout=SOCKET_SEND_TIMEOUT_MS;
	ocl->socketRecvTimeout=SOCKET_RECV_TIMEOUT_MS;
	ocl->responseSpeed=RESPONSE_SPEED;
	ocl->model="";
	ocl->systemRole=malloc(1);
	ocl->systemRole[0]=0;
	ocl->maxMessageContext=MAX_HISTORY_CONTEXT;
	ocl->temp=TEMP;
	ocl->maxTokens=MAX_TOKENS;
	ocl->maxTokensContext=NUM_CTX;
	ocl->contextFile=NULL;
	ocl->showResponseInfo=FALSE;
	return ocl;
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
		free(ocl);
	}
	OCl_flush_context();
	return RETURN_OK;
}

int OCl_load_modelfile(OCl *ocl, char *modelfile){
	ssize_t chars=0;
	size_t len=0;
	char *line=NULL;
	FILE *f=fopen(modelfile,"r");
	if(f==NULL) return ERR_MODEL_FILE_NOT_FOUND;
	while((chars=getline(&line, &len, f))!=-1){
		if((strstr(line,"[MODEL]"))==line){
			chars=getline(&line, &len, f);
			ocl->model=malloc(chars);
			memset(ocl->model,0,chars);
			for(int i=0;i<chars-1;i++) ocl->model[i]=line[i];
			continue;
		}
		if((strstr(line,"[TEMP]"))==line){
			chars=getline(&line, &len, f);
			char *tail=NULL;
			ocl->temp=strtod(line,&tail);
			if(ocl->temp<=0.0 || strcmp(tail,"\n")!=0) return ERR_TEMP;
			continue;
		}
		if((strstr(line,"[MAX_MSG_CTX]"))==line){
			chars=getline(&line, &len, f);
			char *tail=NULL;
			ocl->maxMessageContext=strtol(line,&tail,10);
			if(ocl->maxMessageContext<0 || strcmp(tail,"\n")!=0) return ERR_MAX_MSG_CTX;
			continue;
		}
		if((strstr(line,"[MAX_TOKENS_CTX]"))==line){
			chars=getline(&line, &len, f);
			char *tail=NULL;
			ocl->maxTokensContext=strtol(line,&tail,10);
			if(ocl->maxTokensContext<0 || strcmp(tail,"\n")!=0) return ERR_MAX_TOKENS_CTX;
			continue;
		}
		if((strstr(line,"[MAX_TOKENS]"))==line){
			chars=getline(&line, &len, f);
			char *tail=NULL;
			ocl->maxTokens=strtol(line,&tail,10);
			if(ocl->maxTokens<0 || strcmp(tail,"\n")!=0) return ERR_MAX_TOKENS;
			continue;
		}
		if((strstr(line,"[SYSTEM_ROLE]"))==line){
			while((chars=getline(&line, &len, f))!=-1){
				ocl->systemRole=realloc(ocl->systemRole,strlen(ocl->systemRole)+chars+1);
				strcat(ocl->systemRole,line);
			}
			continue;
		}
	}
	fclose(f);
	free(line);
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
	if(getaddrinfo(srvAddr, NULL, &hints, &res)!=0) return ERR_GETTING_HOST_INFO_ERROR;
	struct sockaddr_in *ipv4=(struct sockaddr_in *)res->ai_addr;
	void *addr=&(ipv4->sin_addr);
	inet_ntop(res->ai_family, addr, ollamaServerIp, sizeof(ollamaServerIp));
	freeaddrinfo(res);
	int socketConn=0;
	struct sockaddr_in serverAddress;
	serverAddress.sin_family=AF_INET;
	serverAddress.sin_port=htons(srvPort);
	serverAddress.sin_addr.s_addr=inet_addr(ollamaServerIp);
	if((socketConn=socket(AF_INET, SOCK_STREAM, 0))<0) return ERR_SOCKET_CREATION_ERROR;
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
	if(select(socketConn+1,&rFdset,&wFdset,NULL,&tv)<=0) return ERR_SOCKET_CONNECTION_TIMEOUT_ERROR;
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
		if(f==NULL) return ERR_OPENING_FILE_ERROR;
		fprintf(f,"%s\t%s\n",userMessage,assistantMessage);
		fclose(f);
	}
	return RETURN_OK;
}

int OCl_import_context(OCl *ocl){
	if(ocl->contextFile!=NULL){
		FILE *f=fopen(ocl->contextFile,"r");
		if(f==NULL) return ERR_OPENING_FILE_ERROR;
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
	case ERR_MALLOC_ERROR:
		snprintf(error_hndl, 1024,"Malloc() error: %s", strerror(errno));
		break;
	case ERR_REALLOC_ERROR:
		snprintf(error_hndl, 1024,"Realloc() error: %s", strerror(errno));
		break;
	case ERR_GETTING_HOST_INFO_ERROR:
		snprintf(error_hndl, 1024,"Error getting host info: %s", strerror(errno));
		break;
	case ERR_SOCKET_CREATION_ERROR:
		snprintf(error_hndl, 1024,"Error creating socket: %s", strerror(errno));
		break;
	case ERR_SOCKET_CONNECTION_TIMEOUT_ERROR:
		snprintf(error_hndl, 1024,"Socket connection time out. ");
		break;
	case ERR_SSL_CONTEXT_ERROR:
		snprintf(error_hndl, 1024,"Error creating SSL context: %s. SSL Error: %s", strerror(errno),ERR_error_string(sslErr, NULL));
		break;
	case ERR_SSL_CERT_NOT_FOUND:
		snprintf(error_hndl, 1024,"SSL cert. not found: %s. SSL Error: %s", strerror(errno),ERR_error_string(sslErr, NULL));
		break;
	case ERR_SSL_FD_ERROR:
		snprintf(error_hndl, 1024,"SSL fd error: %s. SSL Error: %s", strerror(errno),ERR_error_string(sslErr, NULL));
		break;
	case ERR_SSL_CONNECT_ERROR:
		snprintf(error_hndl, 1024,"SSL Connection error: %s. SSL Error: %s", strerror(errno),ERR_error_string(sslErr, NULL));
		break;
	case ERR_SOCKET_SEND_TIMEOUT_ERROR:
		snprintf(error_hndl, 1024,"Sending packet time out. ");
		break;
	case ERR_SENDING_PACKETS_ERROR:
		snprintf(error_hndl, 1024,"Sending packet error. SSL Error: %s", ERR_error_string(sslErr, NULL));
		break;
	case ERR_POLLIN_ERROR:
		snprintf(error_hndl, 1024,"Pollin error. ");
		break;
	case ERR_SOCKET_RECV_TIMEOUT_ERROR:
		snprintf(error_hndl, 1024,"Receiving packet time out. ");
		break;
	case ERR_RECV_TIMEOUT_ERROR:
		snprintf(error_hndl, 1024,"Time out value not valid. ");
		break;
	case ERR_RECEIVING_PACKETS_ERROR:
		snprintf(error_hndl, 1024,"Receiving packet error. SSL Error: %s",ERR_error_string(sslErr, NULL));
		break;
	case ERR_RESPONSE_MESSAGE_ERROR:
		snprintf(error_hndl, 1024,"Error message into JSON. ");
		break;
	case ERR_ZEROBYTESRECV_ERROR:
		snprintf(error_hndl, 1024,"Zero bytes received. Try again...");
		break;
	case ERR_MODEL_FILE_NOT_FOUND:
		snprintf(error_hndl, 1024,"Model file not found. ");
		break;
	case ERR_CONTEXT_FILE_NOT_FOUND:
		snprintf(error_hndl, 1024,"Context file not found. ");
		break;
	case ERR_CERT_FILE_NOT_FOUND:
		snprintf(error_hndl, 1024,"Cert. file not found. ");
		break;
	case ERR_OPENING_FILE_ERROR:
		snprintf(error_hndl, 1024,"Error opening file: %s", strerror(errno));
		break;
	case ERR_OPENING_ROLE_FILE_ERROR:
		snprintf(error_hndl, 1024,"Error opening 'Role' file: %s", strerror(errno));
		break;
	case ERR_NO_HISTORY_CONTEXT_ERROR:
		snprintf(error_hndl, 1024,"No message to save. ");
		break;
	case ERR_UNEXPECTED_JSON_FORMAT_ERROR:
		snprintf(error_hndl, 1024,"Unexpected JSON format error. ");
		break;
	case ERR_CONTEXT_MSGS_ERROR:
		snprintf(error_hndl, 1024,"'Max. Context Message' value out-of-boundaries. ");
		break;
	case ERR_NULL_STRUCT_ERROR:
		snprintf(error_hndl, 1024,"ChatGPT structure null. ");
		break;
	case ERR_SERVICE_UNAVAILABLE:
		snprintf(error_hndl, 1024,"Service unavailable. ");
		break;
	case ERR_LOADING_MODEL:
		snprintf(error_hndl, 1024,"Error loading model. ");
		break;
	case ERR_UNLOADING_MODEL:
		snprintf(error_hndl, 1024,"Error unloading model. ");
		break;
	case ERR_SERVER_ADDR:
		snprintf(error_hndl, 1024,"Server address not valid. ");
		break;
	case ERR_PORT:
		snprintf(error_hndl, 1024,"Port not valid. ");
		break;
	case ERR_TEMP:
		snprintf(error_hndl, 1024,"Temperature value not valid. Check modfile.");
		break;
	case ERR_MAX_MSG_CTX:
		snprintf(error_hndl, 1024,"Max. message context value not valid. Check modfile.");
		break;
	case ERR_MAX_TOKENS_CTX:
		snprintf(error_hndl, 1024,"Max. tokens context value not valid. Check modfile.");
		break;
	case ERR_MAX_TOKENS:
		snprintf(error_hndl, 1024,"Max. tokens value not valid. Check modfile.");
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
	for(int i=strlen(token);(message[i-1]=='\\' || message[i]!=endChar);i++,cont++) (result)[cont]=message[i];
	return RETURN_OK;
}

static int parse_input(char **stringTo, char *stringFrom){
	int cont=0, contEsc=0;
	for(int i=0;i<strlen(stringFrom);i++){
		switch(stringFrom[i]){
		case '\"':
		case '\\':
		case '\n':
		case '\t':
		case '\r':
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
		case '\\':
			(*stringTo)[cont]='\\';
			(*stringTo)[++cont]=stringFrom[i];
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

static void print_response(char *response, long int responseVelocity){
	char *buffer=NULL;
	parse_output(&buffer, response);
	if(responseVelocity==0){
		printf("%s",buffer);
	}else{
		for(int i=0;buffer[i]!=0 && !canceled;i++){
			usleep(responseVelocity);
			printf("%c",buffer[i]);
			fflush(stdout);
		}
	}
	free(buffer);
}

static int send_message(OCl *ocl,char *payload, char **fullResponse, char **content, bool streamed){
	int socketConn=create_connection(ocl->srvAddr, ocl->srvPort, ocl->socketConnectTimeout);
	if(socketConn<0) return ERR_SOCKET_CREATION_ERROR;
	SSL *sslConn=NULL;
	if((sslConn=SSL_new(sslCtx))==NULL){
		clean_ssl(sslConn);
		return ERR_SSL_CONTEXT_ERROR;
	}
	if(!SSL_set_fd(sslConn, socketConn)){
		clean_ssl(sslConn);
		return ERR_SSL_FD_ERROR;
	}
	SSL_set_connect_state(sslConn);
	SSL_set_tlsext_host_name(sslConn, ocl->srvAddr);
	if(!SSL_connect(sslConn)){
		clean_ssl(sslConn);
		return ERR_SSL_CONNECT_ERROR;
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
		return ERR_SOCKET_SEND_TIMEOUT_ERROR;
	}
	pollinHappened=pfds[0].revents & POLLOUT;
	if(pollinHappened){
		int totalBytesSent=0;
		while(totalBytesSent<strlen(payload)){
			bytesSent=SSL_write(sslConn, payload + totalBytesSent, strlen(payload) - totalBytesSent);
			if(bytesSent<=0){
				close(socketConn);
				clean_ssl(sslConn);
				if(bytesSent==0) return ERR_ZEROBYTESRECV_ERROR;
				return ERR_SENDING_PACKETS_ERROR;
			}
			totalBytesSent+=bytesSent;
		}
	}else{
		close(socketConn);
		clean_ssl(sslConn);
		return ERR_POLLIN_ERROR;
	}
	ssize_t bytesReceived=0,totalBytesReceived=0;
	pfds[0].events=POLLIN;
	numEvents=poll(pfds, 1, ocl->socketRecvTimeout);
	if(numEvents==0){
		close(socketConn);
		clean_ssl(sslConn);
		return ERR_SOCKET_RECV_TIMEOUT_ERROR;
	}
	pollinHappened = pfds[0].revents & POLLIN;
	if (pollinHappened){
		*fullResponse=malloc(1);
		(*fullResponse)[0]=0;
		if(content!=NULL){
			*content=malloc(1);
			(*content)[0]=0;
		}
		printf("%s",Colors.h_white);
		do{
			char buffer[BUFFER_SIZE_16K]="";
			bytesReceived=SSL_read(sslConn,buffer, BUFFER_SIZE_16K);
			if(bytesReceived>0){
				totalBytesReceived+=bytesReceived;
				char result[BUFFER_SIZE_16K]="";
				if(streamed){
					get_string_from_token(buffer, "\"content\":\"", result, '"');
					print_response(result, ocl->responseSpeed);
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
				return ERR_RECEIVING_PACKETS_ERROR;
			}

		}while(TRUE && !canceled);
	}
	clean_ssl(sslConn);
	printf("%s",Colors.def);
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
				return ERR_MALLOC_ERROR;
			}
			memset(buf,0,len);
			snprintf(buf,len,contextTemplate,temp->userMessage,temp->assistantMessage);
			context=realloc(context, strlen(context)+strlen(buf)+1);
			if(context==NULL){
				free(messageParsed);
				free(context);
				free(buf);
				return ERR_REALLOC_ERROR;
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
			"Content-Type: application/json\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",ocl->srvAddr,(int) strlen(body), body);
	free(body);
	char *fullResponse=NULL, *content=NULL;
	int resp=send_message(ocl, msg, &fullResponse, &content, TRUE);
	free(msg);
	if(resp<0){
		free(messageParsed);
		free(fullResponse);
		free(content);
		return resp;
	}
	if(strstr(fullResponse,"{\"error")!=NULL){
		printf("%s\n%s%s",Colors.h_red, strstr(fullResponse,"{\"error"),Colors.def);
		return ERR_RESPONSE_MESSAGE_ERROR;
	}
	if(!canceled && resp>0){
		if(ocl->showResponseInfo){
			printf("%s\n\n",Colors.yellow);
			char result[1024]="";
			memset(result,0,1024);
			get_string_from_token(fullResponse, "\"load_duration\":", result, ',');
			printf("- load_duration (time spent loading the model): %f\n",strtod(result,NULL)/1000000000.0);

			memset(result,0,1024);
			get_string_from_token(fullResponse, "\"prompt_eval_duration\":", result, ',');
			printf("- prompt_eval_duration (time spent evaluating the prompt): %f\n",strtod(result,NULL)/1000000000.0);

			memset(result,0,1024);
			get_string_from_token(fullResponse, "\"eval_duration\":", result, '}');
			double ed=strtod(result,NULL)/1000000000.0;
			printf("- eval_duration (time spent generating the response): %f\n",ed);

			memset(result,0,1024);
			get_string_from_token(fullResponse, "\"total_duration\":", result, ',');
			printf("- total_duration (time spent generating the response): %f\n",strtod(result,NULL)/1000000000.0);

			memset(result,0,1024);
			get_string_from_token(fullResponse, "\"prompt_eval_count\":", result, ',');
			printf("- prompt_eval_count (number of tokens in the prompt): %ld\n",strtol(result,NULL,10));

			memset(result,0,1024);
			get_string_from_token(fullResponse, "\"eval_count\":", result, ',');
			long int ec=strtol(result,NULL,10);
			printf("- eval_count (number of tokens in the response): %ld\n",ec);

			printf("- Tokens per sec.: %.2f",ec/ed);
		}
		if(message[strlen(message)-1]!=';'){
			char *buf=NULL;
			parse_input(&buf, content);
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
		return ERR_SERVICE_UNAVAILABLE;
	}
	printf("%s%s\n%s",Colors.h_white, strstr(buffer,"Ollama"),Colors.def);
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
	if(load){
		printf("%s\n%s%s",Colors.h_white, "Loading model... ",Colors.def);
	}else{
		printf("%s\n%s%s",Colors.h_white, "Unloading model... ",Colors.def);
	}
	fflush(stdout);
	int retVal=0;
	if((retVal=send_message(ocl, msg, &buffer, NULL, FALSE))<0) return retVal;
	if(strstr(buffer,"{\"error")!=NULL){
		printf("%s\n\n%s%s",Colors.h_red, strstr(buffer,"{\"error"),Colors.def);
		free(buffer);
		return ERR_LOADING_MODEL;
	}
	if(strstr(buffer,"200 OK")!=NULL){
		printf("%s%s\n%s",Colors.h_green, "OK",Colors.def);
		free(buffer);
		return RETURN_OK;
	}
	free(buffer);
	return ERR_LOADING_MODEL;
}



