/*
 ============================================================================
 Name        : libGPT.c
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
#include <espeak-ng/speak_lib.h>

#define BUFFER_SIZE_16K						(1024*16)

typedef struct Message{
	char *userMessage;
	char *assistantMessage;
	bool isNew;
	struct Message *nextMessage;
}Message;

Message *rootContextMessages=NULL;
int contContextMessages=0;

static void log_debug(char *s){
	FILE *f=fopen(DBG_FILE,"a");
	fprintf(f,"%s\n", s);
	fclose(f);
}

static int create_connection(){
	static char ollamaServerIp[INET_ADDRSTRLEN]="";
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_STREAM;
	if(getaddrinfo(settingInfo.srvAddr, NULL, &hints, &res)!=0) return ERR_GETTING_HOST_INFO_ERROR;
	struct sockaddr_in *ipv4=(struct sockaddr_in *)res->ai_addr;
	void *addr=&(ipv4->sin_addr);
	inet_ntop(res->ai_family, addr, ollamaServerIp, sizeof(ollamaServerIp));
	freeaddrinfo(res);
	int socketConn=0;
	struct sockaddr_in serverAddress;
	serverAddress.sin_family=AF_INET;
	serverAddress.sin_port=htons(settingInfo.srvPort);
	serverAddress.sin_addr.s_addr=inet_addr(settingInfo.srvAddr);
	if((socketConn=socket(AF_INET, SOCK_STREAM, 0))<0) return ERR_SOCKET_CREATION_ERROR;
	int socketFlags=fcntl(socketConn, F_GETFL, 0);
	fcntl(socketConn, F_SETFL, socketFlags | O_NONBLOCK);
	connect(socketConn, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
	fd_set rFdset, wFdset;
	struct timeval tv;
	FD_ZERO(&rFdset);
	FD_SET(socketConn, &rFdset);
	wFdset=rFdset;
	tv.tv_sec=settingInfo.socketConnectTimeout;
	tv.tv_usec=0;
	if(select(socketConn+1,&rFdset,&wFdset,NULL,&tv)<=0) return ERR_SOCKET_CONNECTION_TIMEOUT_ERROR;
	fcntl(socketConn, F_SETFL, socketFlags);
	return socketConn;
}

static void create_new_context_message(char *userMessage, char *assistantMessage, bool isNew){
	Message *newMessage=malloc(sizeof(Message));
	newMessage->userMessage=malloc(strlen(userMessage)+1);
	snprintf(newMessage->userMessage,strlen(userMessage)+1,"%s",userMessage);
	newMessage->assistantMessage=malloc(strlen(assistantMessage)+1);
	snprintf(newMessage->assistantMessage,strlen(assistantMessage)+1,"%s",assistantMessage);
	newMessage->isNew=isNew;
	if(rootContextMessages!=NULL){
		if(contContextMessages>=modelInfo.maxHistoryContext){
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


int export_context(){
	FILE *f=fopen(chatFile,"a");
	if(f==NULL) return ERR_OPENING_FILE_ERROR;
	Message *temp=rootContextMessages;
	while(temp!=NULL){
		if(temp->isNew) fprintf(f,"%s\t%s\n",temp->userMessage,temp->assistantMessage);
		temp=temp->nextMessage;
	}
	fclose(f);
	free(temp);
	return RETURN_OK;
}

int import_context(){
	FILE *f=fopen(chatFile,"r");
	if(f==NULL) return ERR_OPENING_FILE_ERROR;
	size_t len=0, i=0, rows=0, initPos=0;
	ssize_t chars=0;
	char *line=NULL, *userMessage=NULL,*assistantMessage=NULL;;
	while((getline(&line, &len, f))!=-1) rows++;
	if(rows>modelInfo.maxHistoryContext) initPos=rows-modelInfo.maxHistoryContext;
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
			create_new_context_message(userMessage, assistantMessage, FALSE);
			free(userMessage);
			free(assistantMessage);
		}
		contRows++;
	}
	free(line);
	fclose(f);
	return RETURN_OK;
}

void print_error(char *msg, char *error, bool exitProgram){
	printf("\n%s%s%s%s\n",Colors.h_red, msg,error,Colors.def);
	if(exitProgram){
		printf("\n");
		exit(EXIT_FAILURE);
	}
}

char * error_handling(int error){
	static char error_hndl[1024]="";
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
		snprintf(error_hndl, 1024,"Error creating SSL context: %s", strerror(errno));
		break;
	case ERR_SSL_FD_ERROR:
		snprintf(error_hndl, 1024,"SSL fd error: %s", strerror(errno));
		break;
	case ERR_SSL_CONNECT_ERROR:
		snprintf(error_hndl, 1024,"SSL Connection error: %s", strerror(errno));
		break;
	case ERR_SOCKET_SEND_TIMEOUT_ERROR:
		snprintf(error_hndl, 1024,"Sending packet time out. ");
		break;
	case ERR_SENDING_PACKETS_ERROR:
		snprintf(error_hndl, 1024,"Sending packet error. ");
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
		snprintf(error_hndl, 1024,"Receiving packet error. ");
		break;
	case ERR_RESPONSE_MESSAGE_ERROR:
		snprintf(error_hndl, 1024,"Error message into JSON. ");
		break;
	case ERR_ZEROBYTESRECV_ERROR:
		snprintf(error_hndl, 1024,"Zero bytes received. Try again...");
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

static int get_chat_response(char *url,char *payload, char **fullResponse){
	int socketConn=create_connection();
	if(socketConn<0){
		print_error("",error_handling(socketConn),FALSE);
		return RETURN_ERROR;
	}
	struct pollfd pfds[1];
	int numEvents=0,pollinHappened=0,bytesSent=0;
	fcntl(socketConn, F_SETFL, O_NONBLOCK);
	pfds[0].fd=socketConn;
	pfds[0].events=POLLOUT;
	numEvents=poll(pfds,1,settingInfo.socketSendTimeout);
	if(numEvents==0){
		close(socketConn);
		return ERR_SOCKET_SEND_TIMEOUT_ERROR;
	}
	pollinHappened=pfds[0].revents & POLLOUT;
	if(pollinHappened){
		int totalBytesSent=0;
		while(totalBytesSent<strlen(payload)){
			bytesSent=send(socketConn, payload + totalBytesSent, strlen(payload) - totalBytesSent,0);
			totalBytesSent+=bytesSent;
		}
		if(bytesSent<=0){
			close(socketConn);
			return ERR_SENDING_PACKETS_ERROR;
		}
	}else{
		close(socketConn);
		return ERR_POLLIN_ERROR;
	}
	ssize_t bytesReceived=0,totalBytesReceived=0;
	pfds[0].events=POLLIN;
	numEvents=poll(pfds, 1, settingInfo.socketRecvTimeout);
	if(numEvents==0){
		close(socketConn);
		return ERR_SOCKET_RECV_TIMEOUT_ERROR;
	}
	pollinHappened = pfds[0].revents & POLLIN;
	if (pollinHappened){
		printf("%s\n",Colors.h_white);
		*fullResponse=malloc(1);
		(*fullResponse)[0]=0;
		do{
			char buffer[BUFFER_SIZE_16K]="";
			bytesReceived=recv(socketConn,buffer, BUFFER_SIZE_16K,MSG_DONTWAIT);
			if(bytesReceived>0){
				totalBytesReceived+=bytesReceived;
				if(strstr(buffer,"\"error\":\"")!=NULL){
					printf("%s\n%s\n%s",Colors.h_red, strstr(buffer,"{"),Colors.def);
					break;
				}
				char result[1024]="";
				get_string_from_token(buffer, "\"content\":\"", result, '"');
				print_response(result, settingInfo.responseSpeed);
				*fullResponse=realloc(*fullResponse,totalBytesReceived+1);
				strcat(*fullResponse,result);
				if(strstr(buffer,"\"done\":false")!=NULL){
					continue;
				}else{
					if(responseInfo){
						printf("%s\n\n",Colors.yellow);

						memset(result,0,1024);
						get_string_from_token(buffer, "\"load_duration\":", result, ',');
						printf("- load_duration (time spent loading the model): %f\n",strtod(result,NULL)/1000000000.0);

						memset(result,0,1024);
						get_string_from_token(buffer, "\"prompt_eval_duration\":", result, ',');
						printf("- prompt_eval_duration (time spent evaluating the prompt): %f\n",strtod(result,NULL)/1000000000.0);

						memset(result,0,1024);
						get_string_from_token(buffer, "\"eval_duration\":", result, '}');
						double ed=strtod(result,NULL)/1000000000.0;
						printf("- eval_duration (time spent generating the response): %f\n",ed);

						memset(result,0,1024);
						get_string_from_token(buffer, "\"total_duration\":", result, ',');
						printf("- total_duration (time spent generating the response): %f\n",strtod(result,NULL)/1000000000.0);

						memset(result,0,1024);
						get_string_from_token(buffer, "\"prompt_eval_count\":", result, ',');
						printf("- prompt_eval_count (number of tokens in the prompt): %ld\n",strtol(result,NULL,10));

						memset(result,0,1024);
						get_string_from_token(buffer, "\"eval_count\":", result, ',');
						long int ec=strtol(result,NULL,10);
						printf("- eval_count (number of tokens in the response): %ld\n",ec);

						printf("- Tokens per sec.: %.2f",ec/ed);
					}
					break;
				}
			}
			if(bytesReceived==0){
				close(socketConn);
				break;
			}
			if(bytesReceived<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) continue;
			if(bytesReceived<0 && (errno!=EAGAIN)){
				close(socketConn);
				return ERR_RECEIVING_PACKETS_ERROR;
			}
		}while(TRUE && !canceled);
	}
	printf("%s\n",Colors.def);
	if(totalBytesReceived==0) return ERR_ZEROBYTESRECV_ERROR;
	return totalBytesReceived;
}

int send_chat(char *message){
	char *messageParsed=NULL;
	parse_input(&messageParsed, message);
	char *context=malloc(1), *buf=NULL;
	context[0]=0;
	if(message[0]!=';'){
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
	parse_input(&roleParsed, modelInfo.role);
	ssize_t len=
			strlen(modelInfo.model)
			+sizeof(modelInfo.temp)
			+sizeof(modelInfo.maxTokens)
			+sizeof(modelInfo.numCtx)
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
			"\"stop\": null,"
			"\"messages\":["
			"{\"role\":\"system\",\"content\":\"%s\"},"
			"%s""{\"role\": \"user\",\"content\": \"%s\"}]}",
			modelInfo.model,
			modelInfo.temp,
			modelInfo.maxTokens,
			modelInfo.numCtx,
			roleParsed,context,
			messageParsed);
	free(context);
	free(roleParsed);
	len=strlen(settingInfo.srvAddr)+sizeof(settingInfo.srvPort)+sizeof((int) strlen(body))+strlen(body)+512;
	char *msg=malloc(len);
	memset(msg,0,len);
	snprintf(msg,len,
			"POST /api/chat HTTP/1.1\r\n"
			"Host: %s:%d\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",settingInfo.srvAddr,settingInfo.srvPort,(int) strlen(body), body);
	free(body);
	log_debug(msg); //TODO
	char *fullResponse=NULL;
	int resp=get_chat_response(settingInfo.srvAddr, msg, &fullResponse);
	free(msg);
	log_debug(fullResponse); //TODO
	if(resp<0){
		free(messageParsed);
		free(fullResponse);
		return resp;
	}
	if(!canceled && resp>0 && message[0]!=';'){
		create_new_context_message(messageParsed, fullResponse, TRUE);
	}
	free(messageParsed);
	free(fullResponse);
	if(resp<=0) return resp;
	return RETURN_OK;
}

static int send_message(char *msg, char *buffer){
	int socketConn=create_connection();
	if(socketConn<0){
		print_error("",error_handling(socketConn),FALSE);
		return RETURN_ERROR;
	}
	int resp=send(socketConn, msg, strlen(msg),0);
	if(resp<=0){
		perror("");
		print_error("",error_handling(resp),FALSE);
		close(socketConn);
		return resp;
	}
	resp=recv(socketConn,buffer, BUFFER_SIZE_16K,0);
	if(resp<0){
		perror("");
		print_error("",error_handling(resp),FALSE);
		close(socketConn);
		return resp;
	}
	return RETURN_OK;
}

int check_service_status(){
	int socketConn=create_connection();
	if(socketConn<0){
		print_error("",error_handling(socketConn),FALSE);
		return RETURN_ERROR;
	}
	char msg[2048]="";
	snprintf(msg,2048,
			"GET / HTTP/1.1\r\n"
			"Host: %s:%d\r\n\r\n",settingInfo.srvAddr,settingInfo.srvPort);
	char buffer[BUFFER_SIZE_16K]="";
	send_message(msg, buffer);
	printf("%s\n%s\n%s",Colors.h_white, strstr(buffer,"Ollama"),Colors.def);
	fflush(stdout);
	close(socketConn);
	return RETURN_OK;
}

int load_model(bool load){
	char body[1024]="";
	if(load){
		snprintf(body,1024,"{\"model\": \"%s\", \"keep_alive\": -1}",modelInfo.model);
	}else{
		snprintf(body,1024,"{\"model\": \"%s\", \"keep_alive\": 0}",modelInfo.model);
	}
	char msg[2048]="";
	snprintf(msg,2048,
			"POST /api/chat HTTP/1.1\r\n"
			"Host: %s:%d\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",settingInfo.srvAddr,settingInfo.srvPort,(int) strlen(body), body);
	char buffer[BUFFER_SIZE_16K]="";
	send_message(msg, buffer);
	if(load){
		if(strstr(buffer,"200 OK")!=NULL){
			printf("%s\n%s\n%s",Colors.h_white, "Model succesfully loaded...",Colors.def);
		}else{
			printf("%s\n%s\n%s",Colors.h_red, "Error loading model...",Colors.def);
		}
		return RETURN_OK;
	}
	if(strstr(buffer,"200 OK")!=NULL){
		printf("%s\n%s\n%s",Colors.h_white, "Model succesfully unloaded...",Colors.def);
	}else{
		printf("%s\n%s\n%s",Colors.h_red, "Error unloading model...",Colors.def);
	}
	return RETURN_OK;
}



