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
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "lib/libOllama-C-lient.h"

#define PROGRAM_NAME					"Ollama-C-lient"
#define PROGRAM_VERSION					"0.0.1-beta"

#define PROMPT_DEFAULT					"-> "
#define BANNER 							printf("\n%s v%s by L. <https://github.com/lucho-a/ollama-c-lient>\n\n",PROGRAM_NAME, PROGRAM_VERSION);

#define	RESPONSE_SPEED					15000

OCl *ocl=NULL;

bool exitProgram=false, showResponseInfo=false, showThoughts=false, stdinPresent=false;
int prevInput=0;
char systemFont[16]="", promptFont[16]="", errorFont[16]="", showResponseInfoFont[16]="";
char model[512]="";
char temp[512]="";
char keepalive[512]="";
char maxMsgCtx[512]="";
char maxTokensCtx[512]="";
char *systemRole=NULL;
char *contextFile=NULL;
char *fixedContextFile=NULL;
char serverAddr[16]="";
char serverPort[6]="";
long int responseSpeed=RESPONSE_SPEED;
char socketConnTo[512]="";
char socketSendTo[512]="";
char socketRecvTo[512]="";
char responseFont[16]="";

static void print_error_msg(char *msg, char *error, bool exitProgram){
	char errMsg[1024]="";
	snprintf(errMsg,1024,"%sERROR: %s %s\n",errorFont, msg,error);
	for(size_t i=0;i<strlen(errMsg);i++){
		printf("%c",errMsg[i]);
		fflush(stdout);
		usleep(responseSpeed);
	}
	if(exitProgram){
		printf("\e[0m\n");
		exit(EXIT_FAILURE);
	}
}

static void print_system_msg(char *msg){
	char sysMsg[1024]="";
	snprintf(sysMsg,1024,"%s%s\n",systemFont,msg);
	for(size_t i=0;i<strlen(sysMsg);i++){
		printf("%c",sysMsg[i]);
		fflush(stdout);
		usleep(responseSpeed);
	}
}

static int load_settingfile(char const *settingfile){
	size_t len=0;
	char *line=NULL;
	FILE *f=fopen(settingfile,"r");
	if(f==NULL) print_error_msg("Setting file Not Found.","",true);
	while((getline(&line, &len, f))!=-1){
		if((strstr(line,"[SERVER_ADDR]"))==line){
			getline(&line, &len, f);
			line[strlen(line)-1]=0;
			snprintf(serverAddr,16,"%s",line);
			continue;
		}
		if((strstr(line,"[SERVER_PORT]"))==line){
			getline(&line, &len, f);
			line[strlen(line)-1]=0;
			snprintf(serverPort,6,"%s",line);
			continue;
		}
		if((strstr(line,"[RESPONSE_SPEED_MS]"))==line){
			getline(&line, &len, f);
			char *tail=NULL;
			responseSpeed=strtol(line, &tail, 10);
			if(responseSpeed<0||(tail[0]!=0 && tail[0]!='\n')){
				free(line);
				fclose(f);
				return OCL_ERR_RESPONSE_SPEED_NOT_VALID;
			}
			continue;
		}
		if((strstr(line,"[SOCKET_CONNECT_TO_S]"))==line){
			int chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) socketConnTo[i]=line[i];
			continue;
		}
		if((strstr(line,"[SOCKET_SEND_TO_S]"))==line){
			int chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) socketSendTo[i]=line[i];
			continue;
		}
		if((strstr(line,"[SOCKET_RECV_TO_S]"))==line){
			int chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) socketRecvTo[i]=line[i];
			continue;
		}
		if((strstr(line,"[SYSTEM_FONT]"))==line){
			getline(&line, &len, f);
			int f1,f2,color;
			sscanf(line, "%d;%d;%d", &f1, &f2, &color);
			snprintf(systemFont,16,"\e[%d;%d;%dm",f1, f2, color);
			continue;
		}
		if((strstr(line,"[PROMPT_FONT]"))==line){
			getline(&line, &len, f);
			int f1,f2,color;
			sscanf(line, "%d;%d;%d", &f1, &f2, &color);
			snprintf(promptFont,16,"\e[%d;%d;%dm",f1, f2, color);
			continue;
		}
		if((strstr(line,"[RESPONSE_FONT]"))==line){
			getline(&line, &len, f);
			int f1,f2,color;
			sscanf(line, "%d;%d;%d", &f1, &f2, &color);
			snprintf(responseFont,16,"\e[%d;%d;%dm",f1, f2, color);
			continue;
		}
		if((strstr(line,"[ERROR_FONT]"))==line){
			getline(&line, &len, f);
			int f1,f2,color;
			sscanf(line, "%d;%d;%d", &f1, &f2, &color);
			snprintf(errorFont,16,"\e[%d;%d;%dm",f1, f2, color);
			continue;
		}
		if((strstr(line,"[INFO_RESPONSE_FONT]"))==line){
			getline(&line, &len, f);
			int f1,f2,color;
			sscanf(line, "%d;%d;%d", &f1, &f2, &color);
			snprintf(showResponseInfoFont,16,"\e[%d;%d;%dm",f1, f2, color);
			continue;
		}
	}
	fclose(f);
	free(line);
	return OCL_RETURN_OK;
}

static int load_modelfile(char const *modelfile){
	ssize_t chars=0;
	size_t len=0;
	char *line=NULL;
	FILE *f=fopen(modelfile,"r");
	if(f==NULL) print_error_msg("Model file not found.","",true);
	while((getline(&line, &len, f))!=-1){
		if((strstr(line,"[MODEL]"))==line){
			chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) model[i]=line[i];
			continue;
		}
		if((strstr(line,"[TEMP]"))==line){
			chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) temp[i]=line[i];
			continue;
		}
		if((strstr(line,"[KEEP_ALIVE]"))==line){
			chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) keepalive[i]=line[i];
			continue;
		}
		if((strstr(line,"[MAX_MSG_CTX]"))==line){
			chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) maxMsgCtx[i]=line[i];
			continue;
		}
		if((strstr(line,"[MAX_TOKENS_CTX]"))==line){
			chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) maxTokensCtx[i]=line[i];
			continue;
		}
		if((strstr(line,"[SYSTEM_ROLE]"))==line){
			systemRole=malloc(1);
			systemRole[0]=0;
			while((chars=getline(&line, &len, f))!=-1){
				systemRole=realloc(systemRole,strlen(systemRole)+chars+1);
				strcat(systemRole,line);
			}
			continue;
		}
	}
	fclose(f);
	free(line);
	return OCL_RETURN_OK;
}

static int close_program(){
	oclCanceled=true;
	OCl_free(ocl);
	if(systemRole!=NULL) free(systemRole);
	rl_clear_history();
	return OCL_RETURN_OK;
}

static int readline_input(FILE *stream){
	int c=fgetc(stream);
	if(c==13 && prevInput==27){
		if(strlen(rl_line_buffer)==0){
			exitProgram=true;
			rl_done=true;
			return 13;
		}
		rl_insert_text("\n");
		prevInput=0;
		return 0;
	}
	if(c==9) rl_insert_text("\t");
	if(c==-1 || c==4){
		rl_delete_text(0,strlen(rl_line_buffer));
		rl_redisplay();
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
	return lineRead;
}

static int validate_file(char const *file){
	if(file==NULL) return OCL_RETURN_ERROR;
	FILE *f=fopen(file,"r");
	if(f==NULL) return OCL_RETURN_ERROR;
	fclose(f);
	return OCL_RETURN_OK;
}

static void signal_handler(int signalType){
	switch(signalType){
	case SIGSEGV:
	case SIGINT:
	case SIGTSTP:
		oclCanceled=true;
		break;
	case SIGHUP:
		close_program(ocl);
		break;
	case SIGPIPE:
	default:
		break;
	}
}

static void print_response_info(){
	printf("%s\n\n", showResponseInfoFont);
	printf("- load_duration (time spent loading the model): %f\n",OCL_get_response_load_duration(ocl));
	printf("- prompt_eval_duration (time spent evaluating the prompt): %f\n",OCL_get_response_prompt_eval_duration(ocl));
	printf("- eval_duration (time spent generating the response): %f\n",OCL_get_response_eval_duration(ocl));
	printf("- total_duration (time spent generating the response): %f\n",OCL_get_response_total_duration(ocl));
	printf("- prompt_eval_count (number of tokens in the prompt): %d\n",OCL_get_response_prompt_eval_count(ocl));
	printf("- eval_count (number of tokens in the response): %d\n",OCL_get_response_eval_count(ocl));
	printf("- Tokens per sec.: %.2f",OCL_get_response_tokens_per_sec(ocl));
}

bool isThinking=false;

static void print_response(char const *token){
	if(stdinPresent) return;
	printf("%s",responseFont);
	if(strstr(token, "\\u003cthink\\u003e")!=NULL){
		isThinking=true;
		printf("\x1b[3m(Thinking...)\x1b[23m\n");
		if(showThoughts) isThinking=false;
		return;
	}
	if(strstr(token, "\\u003c/think\\u003e")!=NULL){
		isThinking=false;
		printf("\x1b[3m(Stop thinking...)\x1b[23m");
		return;
	}
	if(isThinking) return;
	for(size_t i=0;i<strlen(token);i++){
		usleep(responseSpeed);
		if(token[i]=='\\'){
			switch(token[i+1]){
			case 'n':
				printf("\n");
				break;
			case 'r':
				printf("\r");
				break;
			case 't':
				printf("\t");
				break;
			case '\\':
				printf("\\");
				break;
			case '"':
				printf("\"");
				break;
			case 'u':
				char buffer[5]="";
				snprintf(buffer,5,"%c%c%c%c",token[i+2],token[i+3],token[i+4],token[i+5]);
				printf("%c",(int)strtol(buffer,NULL,16));
				i+=4;
				break;
			default:
				break;
			}
			i++;
			continue;
		}
		printf("%c",token[i]);
		fflush(stdout);
	}
}

void *start_sending_message(void *arg){
	char const *messagePrompted=arg;
	int retVal=OCl_send_chat(ocl,messagePrompted, print_response);
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
		print_system_msg("\nLoading model..");
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
	const char *rolesFile=NULL, *instructionsFile=NULL;
	char buffer[1024]="", input[1024*32]="";
	if(!isatty(fileno(stdin))){
		while(fgets(buffer, sizeof(buffer), stdin)) strcat(input,buffer);
		stdinPresent=true;
	}
	int retVal=0;
	if((OCl_init())!=OCL_RETURN_OK) print_error_msg("OCl init error. ","",true);
	for(int i=1;i<argc;i++){
		if(strcmp(argv[i],"--version")==0 || strcmp(argv[i],"--help")==0){
			BANNER
			exit(EXIT_SUCCESS);
		}
		if(strcmp(argv[i],"--server-addr")==0){
			snprintf(serverAddr,16,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--server-port")==0){
			snprintf(serverPort,6,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--model-file")==0){
			if(validate_file(argv[i+1])!=OCL_RETURN_OK) print_error_msg("Model file not found.","",true);
			if((load_modelfile(argv[i+1]))!=OCL_RETURN_OK) print_error_msg("Loading model file error. ","",true);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--setting-file")==0){
			if(validate_file(argv[i+1])!=OCL_RETURN_OK) print_error_msg("Setting file not found.","",true);
			if((load_settingfile(argv[i+1]))!=OCL_RETURN_OK) print_error_msg("Loading setting file error. ","",true);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--roles-file")==0){
			if(validate_file(argv[i+1])!=OCL_RETURN_OK) print_error_msg("Roles file not found.","",true);
			rolesFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--instructions-file")==0){
			if(validate_file(argv[i+1])!=OCL_RETURN_OK) print_error_msg("Instructions file not found.","",true);
			instructionsFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--context-file")==0){
			if(validate_file(argv[i+1])!=OCL_RETURN_OK) print_error_msg("Context file not found.","",true);
			contextFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--fixed-context-file")==0){
			if(validate_file(argv[i+1])!=OCL_RETURN_OK) print_error_msg("Fixe context file not found.","",true);
			fixedContextFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--show-response-info")==0){
			showResponseInfo=true;
			continue;
		}
		if(strcmp(argv[i],"--show-thoughts")==0){
			showThoughts=true;
			continue;
		}
		printf("\n");
		print_error_msg(argv[i],": argument not recognized",true);
	}
	if((retVal=OCl_get_instance(&ocl, serverAddr, serverPort, socketConnTo, socketSendTo, socketRecvTo, model, keepalive,systemRole,
			maxMsgCtx, temp, maxTokensCtx, contextFile))!=OCL_RETURN_OK) print_error_msg("OCl getting instance error. ",OCL_error_handling(ocl,retVal),true);
	if((retVal=OCl_import_fixed_context(fixedContextFile))!=OCL_RETURN_OK) print_error_msg("Importing fixed context error. ",OCL_error_handling(ocl,retVal),true);
	if((retVal=OCl_import_context(ocl))!=OCL_RETURN_OK) print_error_msg("Importing context error. ",OCL_error_handling(ocl,retVal),true);
	rl_getc_function=readline_input;
	if(stdinPresent){
		pthread_t tSendingMessage;
		pthread_create(&tSendingMessage, NULL, start_sending_message, input);
		pthread_join(tSendingMessage,NULL);
		printf("%s\n",OCL_get_response(ocl));
		close_program(ocl);
		exit(EXIT_SUCCESS);
	}
	char *messagePrompted=NULL;
	do{
		exitProgram=oclCanceled=false;
		free(messagePrompted);
		printf("%s\n",promptFont);
		messagePrompted=readline_get(PROMPT_DEFAULT, false);
		if(exitProgram){
			printf("\n⎋");
			break;
		}
		if(oclCanceled || strcmp(messagePrompted,"")==0) continue;
		printf("↵\n");
		if(strcmp(messagePrompted,"flush;")==0){
			OCl_flush_context();
			continue;
		}
		if(strcmp(messagePrompted,"models;")==0){
			char models[512][512]={""};
			int cantModels=OCl_get_models(ocl, models);
			if(cantModels<0) {
				printf("\n");
				print_error_msg(OCL_error_handling(ocl,cantModels),"",false);
				continue;
			}
			for(int i=0;i<cantModels;i++){
				printf("%s\n  - ", systemFont);
				for(size_t j=0;j<strlen(models[i]);j++){
					printf("%c", models[i][j]);
					fflush(stdout);
					usleep(responseSpeed);
				}
			}
			printf("\n");
			continue;
		}
		if(strncmp(messagePrompted,"model;", strlen("model;"))==0){
			if(strcmp(messagePrompted+strlen("model;"),"")==0){
				OCl_set_model(ocl,model);
			}else{
				OCl_set_model(ocl,messagePrompted+strlen("model;"));
			}
			continue;
		}
		if(strncmp(messagePrompted,"roles;", strlen("roles;"))==0){
			if(validate_file(rolesFile)!=OCL_RETURN_OK){
				print_error_msg("Roles file not found.","",false);
				continue;
			}
			FILE *f=fopen(rolesFile,"r");
			size_t len=0;
			char *line=NULL;
			while((getline(&line, &len, f))!=-1){
				if(line[0]=='['){
					printf("%s\n  - ", systemFont);
					for(size_t i=1;i<strlen(line)-2;i++){
						printf("%c", line[i]);
						fflush(stdout);
						usleep(responseSpeed);
					}
				}
			}
			printf("\n");
			continue;
		}
		if(strncmp(messagePrompted,"role;", strlen("role;"))==0){
			if(strcmp(messagePrompted+strlen("role;"),"")==0){
				OCl_set_role(ocl, systemRole);
				continue;
			}
			if(validate_file(rolesFile)!=OCL_RETURN_OK){
				print_error_msg("Roles file not found.","",false);
				continue;
			}
			FILE *f=fopen(rolesFile,"r");
			size_t len=0;
			char *line=NULL;
			bool found=false;
			char role[255]="";
			int contSpaces=0;
			for(int i=strlen("role;");messagePrompted[i]==' ';i++) contSpaces++;
			snprintf(role, 255,"[%s]", messagePrompted+strlen("role;")+contSpaces);
			char *newSystemRole=NULL;
			newSystemRole=malloc(1);
			newSystemRole[0]=0;
			while((getline(&line, &len, f))!=-1){
				if(strncmp(line, role, strlen(role))==0){
					found=true;
					int chars=0;
					while((chars=getline(&line, &len, f))!=-1){
						if(line[0]=='[') break;
						newSystemRole=realloc(newSystemRole,strlen(newSystemRole)+chars+1);
						strcat(newSystemRole,line);
					}
				}
				if(found) break;
			}
			(found)?(OCl_set_role(ocl, newSystemRole)):(print_error_msg("Role not found", "", false));
			fclose(f);
			free(newSystemRole);
			continue;
		}
		if(strncmp(messagePrompted,"instructions;", strlen("instructions;"))==0){
			if(validate_file(instructionsFile)!=OCL_RETURN_OK){
				print_error_msg("Instructions file not found.","",false);
				continue;
			}
			FILE *f=fopen(instructionsFile,"r");
			size_t len=0;
			char *line=NULL;
			while((getline(&line, &len, f))!=-1){
				if(line[0]=='['){
					printf("%s\n  - ", systemFont);
					for(size_t i=1;i<strlen(line)-2;i++){
						printf("%c", line[i]);
						fflush(stdout);
						usleep(responseSpeed);
					}
				}
			}
			printf("\n");
			continue;
		}
		if(strncmp(messagePrompted,"instruction;", strlen("instruction;"))==0){
			if(strcmp(messagePrompted+strlen("instruction;"),"")==0) continue;
			if(validate_file(instructionsFile)!=OCL_RETURN_OK){
				print_error_msg("Instructions file not found.","",false);
				continue;
			}
			FILE *f=fopen(instructionsFile,"r");
			size_t len=0;
			char *line=NULL;
			bool found=false;
			char instruction[255]="";
			int contSpaces=0;
			for(int i=strlen("instruction;");messagePrompted[i]==' ';i++) contSpaces++;
			snprintf(instruction, 255,"[%s]", messagePrompted+strlen("instruction;")+contSpaces);
			while((getline(&line, &len, f))!=-1){
				if(strncmp(line, instruction, strlen(instruction))==0){
					found=true;
					char buffer[8196]="";
					while((getline(&line, &len, f))!=-1){
						if(line[0]=='[') break;
						strcat(buffer, line);
					}
					add_history(buffer);
				}
				if(found) break;
			}
			fclose(f);
			continue;
		}
		if(check_model_loaded()){
			pthread_t tSendingMessage;
			printf("\n");
			pthread_create(&tSendingMessage, NULL, start_sending_message, messagePrompted);
			add_history(messagePrompted);
			pthread_join(tSendingMessage,NULL);
			if(showResponseInfo && !oclCanceled) print_response_info();
			printf("\n");
		}
	}while(true);
	free(messagePrompted);
	close_program();
	printf("%s\n\n","\e[0m");
	exit(EXIT_SUCCESS);
}

