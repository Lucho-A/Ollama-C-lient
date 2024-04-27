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

#include "lib/libOllama-C-lient.h"

#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <errno.h>

#define PROGRAM_NAME					"Ollama-C-lient"
#define PROGRAM_VERSION					"0.0.1"

#define PROMPT_DEFAULT					"-> "
#define BANNER 							printf("\n%s%s v%s by L.%s\n\n",Colors.h_white,PROGRAM_NAME, PROGRAM_VERSION,Colors.def);

bool canceled=FALSE, exitProgram=FALSE, responseInfo=FALSE;
int prevInput=0;
struct Colors Colors;
struct SettingInfo settingInfo;
struct ModelInfo modelInfo;
char *chatFile=NULL;

int init_OCL(){
	settingInfo.srvAddr=OLLAMA_SERVER_ADDR;
	settingInfo.srvPort=OLLAMA_SERVER_PORT;
	settingInfo.socketConnectTimeout=SOCKET_CONNECT_TIMEOUT_S;
	settingInfo.socketSendTimeout=SOCKET_SEND_TIMEOUT_MS;
	settingInfo.socketRecvTimeout=SOCKET_RECV_TIMEOUT_MS;
	settingInfo.responseSpeed=RESPONSE_SPEED;
	modelInfo.model=NULL;
	modelInfo.role=malloc(1);
	modelInfo.role[0]=0;
	modelInfo.maxHistoryContext=MAX_HISTORY_CONTEXT;
	modelInfo.temp=TEMP;
	modelInfo.maxTokens=MAX_TOKENS;
	modelInfo.numCtx=NUM_CTX;
	return RETURN_OK;
}

int close_OCL(){
	export_context();
	load_model(FALSE);
	rl_clear_history();
	printf("%s\n",Colors.def);
	exit(EXIT_SUCCESS);
}

void init_colors(bool uncolored){
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

int setting_model_options(char *modelFile){
	ssize_t chars=0;
	size_t len=0;
	char *line=NULL;
	FILE *f=fopen(modelFile,"r");
	if(f==NULL) print_error("Error opening model file: ",strerror(errno),TRUE);
	char *param=NULL;
	while((chars=getline(&line, &len, f))!=-1){
		if((strstr(line,"[MODEL]"))==line){
			param="model";
			continue;
		}
		if((strstr(line,"[ROLE]"))==line){
			param="role";
			continue;
		}
		if((strstr(line,"[MAX_HISTORY_CONTEXT]"))==line){
			param="maxhistoryctx";
			continue;
		}
		if((strstr(line,"[TEMP]"))==line){
			param="temp";
			continue;
		}
		if((strstr(line,"[MAX_TOKENS]"))==line){
			param="maxtokens";
			continue;
		}
		if((strstr(line,"[NUM_CTX]"))==line){
			param="numctx";
			continue;
		}
		if(strcmp(param,"model")==0){
			modelInfo.model=malloc(chars);
			memset(modelInfo.model,0,chars);
			for(int i=0;i<chars-1;i++) modelInfo.model[i]=line[i];
			continue;
		}
		if(strcmp(param,"role")==0){
			modelInfo.role=realloc(modelInfo.role,strlen(modelInfo.role)+chars+1);
			strcat(modelInfo.role,line);
			continue;
		}
		if(strcmp(param,"maxhistoryctx")==0){
			modelInfo.maxHistoryContext=strtol(line,NULL,10);
			continue;
		}
		if(strcmp(param,"temp")==0){
			modelInfo.temp=strtod(line,NULL);
			continue;
		}
		if(strcmp(param,"maxtokens")==0){
			modelInfo.maxTokens=strtol(line,NULL,10);
			continue;
		}
		if(strcmp(param,"numctx")==0){
			modelInfo.numCtx=strtol(line,NULL,10);
			continue;
		}
	}
	fclose(f);
	free(line);
	return RETURN_OK;
}

int readline_input(FILE *stream){
	int c=fgetc(stream);
	if(c==13 && prevInput==27){
		if(strlen(rl_line_buffer)==0){
			exitProgram=TRUE;
			rl_done=TRUE;
			return 13;
		}
		rl_insert_text("\n");
		prevInput=0;
		return 0;
	}
	if(c==9) rl_insert_text("\t");
	if(c==-1 || c==4){
		rl_delete_text(0,strlen(rl_line_buffer));
		return 13;
	}
	prevInput=c;
	return c;
}

static char *readline_get(const char *prompt, bool addHistory){
	char *lineRead=(char *)NULL;
	if(lineRead){
		free(lineRead);
		lineRead=(char *)NULL;
	}
	lineRead=readline(prompt);
	if(lineRead && *lineRead && addHistory) add_history(lineRead);
	return(lineRead);
}

void signal_handler(int signalType){
	switch(signalType){
	case SIGINT:
	case SIGTSTP:
		canceled=TRUE;
		break;
	case SIGHUP:
		close_OCL();
	case SIGPIPE:
		print_error("'SIGPIPE' signal received: the write end of the pipe or socket is closed.","", FALSE);
		break;
	}
}

int main(int argc, char *argv[]) {
	signal(SIGINT, signal_handler);
	signal(SIGPIPE, signal_handler);
	signal(SIGTSTP, signal_handler);
	signal(SIGHUP, signal_handler);
	char *modelFile=NULL;
	init_OCL();
	init_colors(FALSE);
	for(int i=1;i<argc;i++){
		if(strcmp(argv[i],"--version")==0){
			BANNER;
			exit(EXIT_SUCCESS);
		}
		if(strcmp(argv[i],"--server-addr")==0){
			settingInfo.srvAddr=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--server-port")==0){
			settingInfo.srvPort=strtol(argv[i+1],NULL,10);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--model-file")==0){
			modelFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--chat-file")==0){
			if(argv[i+1]==NULL) print_error("Error opening chat file: ",strerror(errno),TRUE);
			chatFile=argv[i+1];
			FILE *f=fopen(chatFile,"a");
			if(f==NULL) print_error("Error opening chat file: ",strerror(errno),TRUE);
			fclose(f);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--show-response-info")==0){
			responseInfo=TRUE;
			continue;
		}
		print_error(argv[i],": argument not recognized",TRUE);
	}
	if(modelFile==NULL) print_error("Model file not found. ","",TRUE);
	setting_model_options(modelFile);
	import_context();
	if(check_service_status()==RETURN_ERROR){
		printf("%s\n",Colors.def);
		exit(EXIT_FAILURE);
	}
	load_model(TRUE);
	rl_getc_function=readline_input;
	char *messagePrompted=NULL;
	do{
		exitProgram=canceled=FALSE;
		free(messagePrompted);
		printf("%s\n",Colors.h_cyan);
		messagePrompted=readline_get(PROMPT_DEFAULT, FALSE);
		if(exitProgram){
			printf("\n⎋\n%s",Colors.def);
			break;
		}
		if(canceled || strcmp(messagePrompted,"")==0) continue;
		if(strcmp(messagePrompted,"status;")==0){
			check_service_status();
			continue;
		}
		int resp=0;
		printf("↵\n%s",Colors.def);
		if((resp=send_chat(messagePrompted))!=RETURN_OK){
			switch(resp){
			case ERR_RESPONSE_MESSAGE_ERROR:
				break;
			default:
				if(canceled){
					printf("\n");
					break;
				}
				print_error("",error_handling(resp),FALSE);
				break;
			}
		}
		add_history(messagePrompted);
	}while(TRUE);
	free(messagePrompted);
	close_OCL();
}
