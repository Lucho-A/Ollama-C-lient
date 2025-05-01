/*
 ============================================================================
 Name        : Ollama-C-lient.c
 Author      : L. (lucho-a.github.io)
 Version     : 0.0.1
 Created on	 : 2024/04/19
 Copyright   : GNU General Public License v3.0
 Description : Main file
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "lib/libOllama-C-lient.h"

#define PROGRAM_NAME					"Ollama-C-lient"
#define PROGRAM_VERSION					"0.0.1-beta"

#define BANNER 							printf("\n%s v%s by L. <https://github.com/lucho-a/ollama-c-lient>\n\n",PROGRAM_NAME, PROGRAM_VERSION);

#define	RESPONSE_SPEED					0

OCl *ocl=NULL;

struct OclParams{
	char serverAddr[16];
	char serverPort[6];
	char socketConnTo[8];
	char socketSendTo[8];
	char socketRecvTo[8];
	char model[128];
	char temp[8];
	char keepalive[8];
	char maxMsgCtx[8];
	char maxTokensCtx[8];
	char *systemRole;
	char *contextFile;
	char *staticContextFile;
};

struct Colors{
	char colorFontSystem[16];
	char colorFontError[16];
	char colorFontInfo[16];
	char colorFontResponse[16];
};

struct ProgramOpts{
	struct OclParams ocl;
	long int responseSpeed;
	bool showResponseInfo;
	bool showThoughts;
	bool showModels;
	bool showLoadingModels;
	bool stdoutParsed;
	bool stdoutChunked;
	struct Colors colors;
};

struct SendingMessage{
	char *input;
	char *imageFile;
};

struct ProgramOpts po={0};
struct SendingMessage sm={0};
bool isThinking=false;
char chunkings[8196]="";

static int close_program(bool finishWithErrors){
	oclCanceled=true;
	if(ocl) OCl_free(ocl);
	OCl_shutdown();
	if(po.ocl.staticContextFile!=NULL) free(po.ocl.staticContextFile);
	if(po.ocl.contextFile!=NULL) free(po.ocl.contextFile);
	if(po.ocl.systemRole!=NULL) free(po.ocl.systemRole);
	if(sm.input!=NULL) free(sm.input);
	if(isatty(fileno(stdout))) fputs("\x1b[0m\n",stdout);
	if(finishWithErrors) exit(EXIT_FAILURE);
	exit(EXIT_SUCCESS);
}

static void signal_handler(int signalType){
	switch(signalType){
	case SIGSEGV:
	case SIGINT:
	case SIGTSTP:
		oclCanceled=true;
		break;
	case SIGHUP:
		close_program(true);
		break;
	case SIGPIPE:
	default:
		break;
	}
}

static void print_error_msg(char *msg, char *error, bool exitProgram){
	char errMsg[1024]="";
	if(isatty(fileno(stderr))){
		snprintf(errMsg,1024,"%sERROR: %s %s\n",po.colors.colorFontError, msg,error);
	}else{
		snprintf(errMsg,1024,"ERROR: %s %s\n",msg,error);
	}
	for(size_t i=0;i<strlen(errMsg);i++){
		fputc(errMsg[i],stderr);
		fflush(stderr);
		usleep(po.responseSpeed);
	}
	if(isatty(fileno(stderr))) fputs("\x1b[0m\n",stderr);
	if(exitProgram) close_program(true);
}

static void print_system_msg(char *msg){
	char sysMsg[1024]="";
	if(isatty(fileno(stdout))){
		snprintf(sysMsg,1024,"%s%s\n",po.colors.colorFontSystem,msg);
	}else{
		snprintf(sysMsg,1024,"%s\n",msg);
	}
	for(size_t i=0;i<strlen(sysMsg);i++){
		fputc(sysMsg[i],stdout);
		fflush(stdout);
		usleep(po.responseSpeed);
	}
	if(isatty(fileno(stdout))) printf("%s",po.colors.colorFontResponse);
}


static void print_response_info(){
	if(isatty(fileno(stdout))) printf("%s\n\n", po.colors.colorFontInfo);
	printf("- load_duration (time spent loading the model): %f\n",OCL_get_response_load_duration(ocl));
	printf("- prompt_eval_duration (time spent evaluating the prompt): %f\n",OCL_get_response_prompt_eval_duration(ocl));
	printf("- eval_duration (time spent generating the response): %f\n",OCL_get_response_eval_duration(ocl));
	printf("- total_duration (time spent generating the response): %f\n",OCL_get_response_total_duration(ocl));
	printf("- prompt_eval_count (number of tokens in the prompt): %d\n",OCL_get_response_prompt_eval_count(ocl));
	printf("- eval_count (number of tokens in the response): %d\n",OCL_get_response_eval_count(ocl));
	printf("- Tokens per sec.: %.2f",OCL_get_response_tokens_per_sec(ocl));
	if(isatty(fileno(stdout))) printf("%s",po.colors.colorFontResponse);
}

char *parse_output(const char *in){
	char buffer[5]="";
	char *buff=malloc(strlen(in)+1);
	memset(buff,0,strlen(in)+1);
	int cont=0;
	for(size_t i=0;i<strlen(in);i++,cont++){
		if(in[i]=='\\'){
			switch(in[i+1]){
			case 'n':
				buff[cont]='\n';
				break;
			case 'r':
				buff[cont]='\r';
				break;
			case 't':
				buff[cont]='\r';
				break;
			case '\\':
				buff[cont]='\\';
				break;
			case '"':
				buff[cont]='\"';
				break;
			case 'u':
				snprintf(buffer,5,"%c%c%c%c",in[i+2],in[i+3],in[i+4],in[i+5]);
				buff[cont]=(int)strtol(buffer,NULL,16);
				i+=4;
				break;
			default:
				break;
			}
			i++;
			continue;
		}
		buff[cont]=in[i];
	}
	buff[cont]=0;
	return buff;
}

static void print_response(char const *token, bool done){
	if(po.stdoutParsed && po.stdoutChunked){
		char *parsedOut=parse_output(token);
		strcat(chunkings,parsedOut);
		if(strstr(parsedOut, "\n") || done){
			fputs(chunkings, stdout);
			fflush(stdout);
			memset(chunkings,0,8196);
		}
		free(parsedOut);
		return;
	}
	if(po.responseSpeed==0) return;
	if(strstr(token, "\\u003cthink\\u003e")!=NULL){
		isThinking=true;
		fputs("(Thinking...)\n", stdout);
		fflush(stdout);
		if(po.showThoughts) isThinking=false;
		return;
	}
	if(strstr(token, "\\u003c/think\\u003e")!=NULL){
		isThinking=false;
		fputs("(Stop thinking...)\n", stdout);
		fflush(stdout);
		return;
	}
	if(isThinking) return;
	if(po.stdoutParsed){
		char *parsedOut=parse_output(token);
		for(size_t i=0;i<strlen(parsedOut);i++){
			usleep(po.responseSpeed);
			fputc(parsedOut[i], stdout);
			fflush(stdout);
		}
		free(parsedOut);
	}else{
		fputs(token, stdout);
		fflush(stdout);
	}
}

void *start_sending_message(void *arg){
	struct SendingMessage *sm=arg;
	int retVal=OCl_send_chat(ocl,sm->input,sm->imageFile ,print_response);
	if(retVal!=OCL_RETURN_OK){
		if(oclCanceled) printf("\n");
		oclCanceled=true;
		print_error_msg(OCL_error_handling(ocl, retVal),"",false);
	}
	pthread_exit(NULL);
}

bool check_model_loaded(){
	int retVal=0;
	if((retVal=OCl_check_model_loaded(ocl))<=0){
		if(retVal<0){
			print_error_msg(OCL_error_handling(ocl, retVal),"",false);
			return false;
		}
		if(po.showLoadingModels) print_system_msg("Loading model..\n");
		if((retVal=OCl_load_model(ocl, true))<0){
			printf("\n");
			print_error_msg(OCL_error_handling(ocl,retVal),"",false);
			return false;
		}
	}
	return true;
}

int main(int argc, char *argv[]) {
	signal(SIGINT, signal_handler);
	signal(SIGPIPE, signal_handler);
	signal(SIGTSTP, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGSEGV, signal_handler);
	po.responseSpeed=RESPONSE_SPEED;
	if(!isatty(fileno(stdin))){
		char *line=NULL;
		size_t len=0;
		long int chars=0;
		sm.input=malloc(1);
		sm.input[0]=0;
		while((chars=getline(&line, &len, stdin))!=-1){
			sm.input=realloc(sm.input,strlen(sm.input)+chars+1);
			strcat(sm.input,line);
		}
		sm.input[strlen(sm.input)-1]=0;
		free(line);
	}
	int retVal=0;
	if((retVal=OCl_init())!=OCL_RETURN_OK)
		print_error_msg("OCl Init error. ", OCL_error_handling(ocl,retVal),true);
	for(int i=1;i<argc;i++){
		if(strcmp(argv[i],"--version")==0 || strcmp(argv[i],"--help")==0){
			BANNER
			close_program(false);
		}
		if(strcmp(argv[i],"--server-addr")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			snprintf(po.ocl.serverAddr,16,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--server-port")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			snprintf(po.ocl.serverPort,6,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--response-speed")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			char *tail=NULL;
			po.responseSpeed=strtol(argv[i+1], &tail, 10);
			if(po.responseSpeed<0||(tail[0]!=0 && tail[0]!='\n'))
				print_error_msg("Response speed not valid.","",true);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--socket-connect-to")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			snprintf(po.ocl.socketConnTo,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--socket-send-to")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			snprintf(po.ocl.socketSendTo,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--socket-recv-to")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			snprintf(po.ocl.socketRecvTo,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--model")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			snprintf(po.ocl.model,128,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--temperature")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			snprintf(po.ocl.temp,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--keep-alive")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			snprintf(po.ocl.keepalive,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--max-msgs-ctx")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			snprintf(po.ocl.maxMsgCtx,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--max-msgs-tokens")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			snprintf(po.ocl.maxTokensCtx,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--system-role")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			if(po.ocl.systemRole) free(po.ocl.systemRole);
			po.ocl.systemRole=NULL;
			po.ocl.systemRole=malloc(strlen(argv[i+1])+1);
			memset(po.ocl.systemRole,0,strlen(argv[i+1])+1);
			snprintf(po.ocl.systemRole,strlen(argv[i+1])+1,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--system-role-file")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			if(po.ocl.systemRole==NULL){
				FILE *f=fopen(argv[i+1],"r");
				if(f==NULL)print_error_msg("System role file not found.","",true);
				char *line=NULL;
				size_t len=0;
				long int chars=0;
				po.ocl.systemRole=malloc(1);
				po.ocl.systemRole[0]=0;
				while((chars=getline(&line, &len, f))!=-1){
					po.ocl.systemRole=realloc(po.ocl.systemRole,strlen(po.ocl.systemRole)+chars+1);
					strcat(po.ocl.systemRole,line);
				}
				fclose(f);
				free(line);
			}
			i++;
			continue;
		}
		if(strcmp(argv[i],"--image-file")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			sm.imageFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--static-context-file")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			po.ocl.staticContextFile=malloc(strlen(argv[i+1])+1);
			memset(po.ocl.staticContextFile,0,strlen(argv[i+1])+1);
			snprintf(po.ocl.staticContextFile,strlen(argv[i+1])+1,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--context-file")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			po.ocl.contextFile=malloc(strlen(argv[i+1])+1);
			memset(po.ocl.contextFile,0,strlen(argv[i+1])+1);
			snprintf(po.ocl.contextFile,strlen(argv[i+1])+1,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--color-font-response")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			int f1,f2,color;
			sscanf(argv[i+1], "%d;%d;%d", &f1, &f2, &color);
			snprintf(po.colors.colorFontResponse,16,"\e[%d;%d;%dm",f1, f2, color);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--color-font-system")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			int f1,f2,color;
			sscanf(argv[i+1], "%d;%d;%d", &f1, &f2, &color);
			snprintf(po.colors.colorFontSystem,16,"\e[%d;%d;%dm",f1, f2, color);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--color-font-info")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			int f1,f2,color;
			sscanf(argv[i+1], "%d;%d;%d", &f1, &f2, &color);
			snprintf(po.colors.colorFontInfo,16,"\e[%d;%d;%dm",f1, f2, color);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--color-font-error")==0){
			if(!argv[i+1]) print_error_msg("Argument missing","",true);
			int f1,f2,color;
			sscanf(argv[i+1], "%d;%d;%d", &f1, &f2, &color);
			snprintf(po.colors.colorFontError,16,"\e[%d;%d;%dm",f1, f2, color);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--show-response-info")==0){
			po.showResponseInfo=true;
			continue;
		}
		if(strcmp(argv[i],"--show-thoughts")==0){
			po.showThoughts=true;
			continue;
		}
		if(strcmp(argv[i],"--stdout-parsed")==0){
			po.stdoutParsed=true;
			continue;
		}
		if(strcmp(argv[i],"--stdout-chunked")==0){
			po.stdoutChunked=true;
			continue;
		}
		if(strcmp(argv[i],"--show-models")==0){
			po.showModels=true;
			continue;
		}
		if(strcmp(argv[i],"--show-loading-model")==0){
			po.showLoadingModels=true;
			continue;
		}
		print_error_msg(argv[i],": argument not recognized",true);
	}
	if(isatty(fileno(stdout))) printf("%s",po.colors.colorFontResponse);
	if((retVal=OCl_get_instance(
			&ocl,
			po.ocl.serverAddr,
			po.ocl.serverPort,
			po.ocl.socketConnTo,
			po.ocl.socketSendTo,
			po.ocl.socketRecvTo,
			po.ocl.model,
			po.ocl.keepalive,
			po.ocl.systemRole,
			po.ocl.maxMsgCtx,
			po.ocl.temp,
			po.ocl.maxTokensCtx,
			po.ocl.contextFile,
			po.ocl.staticContextFile))!=OCL_RETURN_OK)
		print_error_msg("",OCL_error_handling(ocl,retVal),true);
	if(po.showModels){
		char models[512][512]={""};
		int cantModels=OCl_get_models(ocl, models);
		if(cantModels<0) {
			printf("\n");
			print_error_msg(OCL_error_handling(ocl,cantModels),"",true);
		}
		printf("\n");
		for(int i=0;i<cantModels;i++){
				printf("  - ");
			for(size_t j=0;j<strlen(models[i]);j++){
				fputc(models[i][j], stdout);
				fflush(stdout);
				usleep(po.responseSpeed);
			}
			printf("\n");
		}
	}
	if(sm.input){
		if(check_model_loaded()){
			pthread_t tSendingMessage;
			pthread_create(&tSendingMessage, NULL, start_sending_message, &sm);
			pthread_join(tSendingMessage,NULL);
			free(sm.input);
			sm.input=NULL;
			if(po.responseSpeed==0){
				if(!po.stdoutParsed){
					fputs(OCL_get_response(ocl), stdout);
					fflush(stdout);
				}else{
					if(!po.stdoutChunked){
						char *out=parse_output(OCL_get_response(ocl));
						fputs(out, stdout);
						fflush(stdout);
						free(out);
					}
				}
			}
			if(po.showResponseInfo && !oclCanceled) print_response_info();
			printf("\n");
		}
	}
	close_program(false);
}

