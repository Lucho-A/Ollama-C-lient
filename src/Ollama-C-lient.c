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
#include "lib/libOllama-C-lient.h"

#define PROGRAM_NAME					"Ollama-C-lient"
#define PROGRAM_VERSION					"0.0.1-beta"

#define PROMPT_DEFAULT					"-> "
#define BANNER 							printf("\n%s v%s by L. <https://github.com/lucho-a/ollama-c-lient>\n\n",PROGRAM_NAME, PROGRAM_VERSION);

Bool canceled=FALSE, exitProgram=FALSE, showResponseInfo=FALSE;
int prevInput=0;
OCl *ocl=NULL;

char systemFont[16]="", promptFont[16]="", errorFont[16]="", showResponseInfoFont[16]="";

char model[512]="";
char temp[512]="";
char maxMsgCtx[512]="";
char maxTokensCtx[512]="";
char maxTokens[512]="";
char *systemRole=NULL;
char *contextFile=NULL;
char *serverAddr=NULL;
char *serverPort=NULL;
char responseSpeed[512]="";
char socketConnTo[512]="";
char socketSendTo[512]="";
char socketRecvTo[512]="";
char responseFont[16]="";

static void print_error(char *msg, char *error, Bool exitProgram){
	printf("\n%sERROR: %s %s",errorFont, msg,error);
	if(exitProgram){
		printf("\e[0m\n\n");
		exit(EXIT_FAILURE);
	}
}

static void print_system_msg(char *msg){
	printf("%s%s",systemFont, msg);
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

static int load_settingfile(char *settingfile){
	ssize_t chars=0;
	size_t len=0;
	char *line=NULL;
	FILE *f=fopen(settingfile,"r");
	if(f==NULL) print_error("Setting file Not Found.","",TRUE);
	while((chars=getline(&line, &len, f))!=-1){
		if((strstr(line,"[SERVER_ADDR]"))==line){
			chars=getline(&line, &len, f);
			serverAddr=malloc(chars+1);
			memset(serverAddr,0,chars+1);
			for(int i=0;i<chars-1;i++) serverAddr[i]=line[i];
			continue;
		}
		if((strstr(line,"[SERVER_PORT]"))==line){
			chars=getline(&line, &len, f);
			serverPort=malloc(chars+1);
			memset(serverPort,0,chars+1);
			for(int i=0;i<chars-1;i++) serverPort[i]=line[i];
			continue;
		}
		if((strstr(line,"[RESPONSE_SPEED_MS]"))==line){
			chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) responseSpeed[i]=line[i];
			continue;
		}
		if((strstr(line,"[SOCKET_CONNECT_TO_S]"))==line){
			chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) socketConnTo[i]=line[i];
			continue;
		}
		if((strstr(line,"[SOCKET_SEND_TO_S]"))==line){
			chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) socketSendTo[i]=line[i];
			continue;
		}
		if((strstr(line,"[SOCKET_RECV_TO_S]"))==line){
			chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) socketRecvTo[i]=line[i];
			continue;
		}
		if((strstr(line,"[SYSTEM_FONT]"))==line){
			chars=getline(&line, &len, f);
			int f1,f2,color;
			sscanf(line, "%d;%d;%d", &f1, &f2, &color);
			snprintf(systemFont,16,"\e[%d;%d;%dm",f1, f2, color);
			continue;
		}
		if((strstr(line,"[PROMPT_FONT]"))==line){
			chars=getline(&line, &len, f);
			int f1,f2,color;
			sscanf(line, "%d;%d;%d", &f1, &f2, &color);
			snprintf(promptFont,16,"\e[%d;%d;%dm",f1, f2, color);
			continue;
		}
		if((strstr(line,"[RESPONSE_FONT]"))==line){
			chars=getline(&line, &len, f);
			int f1,f2,color;
			sscanf(line, "%d;%d;%d", &f1, &f2, &color);
			snprintf(responseFont,16,"\e[%d;%d;%dm",f1, f2, color);
			continue;
		}
		if((strstr(line,"[ERROR_FONT]"))==line){
			chars=getline(&line, &len, f);
			int f1,f2,color;
			sscanf(line, "%d;%d;%d", &f1, &f2, &color);
			snprintf(errorFont,16,"\e[%d;%d;%dm",f1, f2, color);
			continue;
		}
		if((strstr(line,"[INFO_RESPONSE_FONT]"))==line){
			chars=getline(&line, &len, f);
			int f1,f2,color;
			sscanf(line, "%d;%d;%d", &f1, &f2, &color);
			snprintf(showResponseInfoFont,16,"\e[%d;%d;%dm",f1, f2, color);
			continue;
		}
	}
	fclose(f);
	free(line);
	return RETURN_OK;
}

static int load_modelfile(char *modelfile){
	ssize_t chars=0;
	size_t len=0;
	char *line=NULL;
	FILE *f=fopen(modelfile,"r");
	if(f==NULL) print_error("Model file not found.","",TRUE);
	while((chars=getline(&line, &len, f))!=-1){
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
		if((strstr(line,"[MAX_TOKENS]"))==line){
			chars=getline(&line, &len, f);
			for(int i=0;i<chars-1;i++) maxTokens[i]=line[i];
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
	return RETURN_OK;
}

static int close_program(OCl *ocl){
	int retVal=0;
	if(strcmp(OCl_get_model(ocl),"")!=0){
		if((retVal=OCl_load_model(ocl,FALSE))!=RETURN_OK){
			print_error(OCL_get_response_error(ocl),OCL_error_handling(retVal),FALSE);
			printf("\n");
		}
	}
	OCl_free(ocl);
	if(serverAddr!=NULL) free(serverAddr);
	if(serverPort!=NULL) free(serverPort);
	if(systemRole!=NULL) free(systemRole);
	rl_clear_history();
	printf("%s\n\n","\e[0m");
	exit(EXIT_SUCCESS);
}

static int readline_input(FILE *stream){
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

static char *readline_get(const char *prompt, Bool addHistory){
	char *lineRead=(char *)NULL;
	if(lineRead){
		free(lineRead);
		lineRead=(char *)NULL;
	}
	lineRead=readline(prompt);
	if(lineRead && *lineRead && addHistory) add_history(lineRead);
	return(lineRead);
}

static int validate_file(char *file){
	if(file==NULL) return RETURN_ERROR;
	FILE *f=fopen(file,"r");
	if(f==NULL) return RETURN_ERROR;
	fclose(f);
	return RETURN_OK;
}

static void signal_handler(int signalType){
	switch(signalType){
	case SIGSEGV:
	case SIGINT:
	case SIGTSTP:
	case SIGPIPE:
		canceled=TRUE;
		break;
	case SIGHUP:
		close_program(ocl);
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[]) {
	signal(SIGINT, signal_handler);
	signal(SIGPIPE, signal_handler);
	signal(SIGTSTP, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGSEGV, signal_handler);
	char *modelFile=NULL, *settingFile=NULL, *rolesFile=NULL;
	int retVal=0;
	if((retVal=OCl_init())!=RETURN_OK) print_error("OCl init error. ",OCL_error_handling(retVal),TRUE);
	for(int i=1;i<argc;i++){
		if(strcmp(argv[i],"--version")==0 || strcmp(argv[i],"--help")==0){
			BANNER;
			exit(EXIT_SUCCESS);
		}
		if(strcmp(argv[i],"--server-addr")==0){
			serverAddr=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--server-port")==0){
			serverPort=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--model-file")==0){
			if(validate_file(argv[i+1])!=RETURN_OK) print_error("Model file not found.","",TRUE);
			modelFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--setting-file")==0){
			if(validate_file(argv[i+1])!=RETURN_OK) print_error("Setting file not found.","",TRUE);
			settingFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--roles-file")==0){
			if(validate_file(argv[i+1])!=RETURN_OK) print_error("Roles file not found.","",TRUE);
			rolesFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--context-file")==0){
			if(validate_file(argv[i+1])!=RETURN_OK) print_error("Context file not found.","",TRUE);
			contextFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--show-response-info")==0){
			showResponseInfo=TRUE;
			continue;
		}
		print_error(argv[i],": argument not recognized",TRUE);
	}
	printf("\n");
	if(modelFile!=NULL){
		if((retVal=load_modelfile(modelFile))!=RETURN_OK)
			print_error("Loading model file error. ",OCL_error_handling(retVal),TRUE);
	}
	if(settingFile!=NULL){
		if((retVal=load_settingfile(settingFile))!=RETURN_OK)
			print_error("Loading setting file error. ",OCL_error_handling(retVal),TRUE);
	}
	if((retVal=OCl_get_instance(&ocl, serverAddr, serverPort, socketConnTo, socketSendTo, socketRecvTo,
			responseSpeed, responseFont, model, systemRole, maxMsgCtx, temp, maxTokens,maxTokensCtx,
			contextFile))!=RETURN_OK) print_error("OCl getting instance error. ",OCL_error_handling(retVal),TRUE);
	if((retVal=OCl_import_context(ocl))!=RETURN_OK)
		print_error("Importing context error. ",OCL_error_handling(retVal),TRUE);
	if((retVal=OCl_check_service_status(ocl))!=RETURN_OK)
		print_error("Service not available. ",OCL_error_handling(retVal),TRUE);
	print_system_msg("Server status: Ollama is running\n");
	if(strcmp(OCl_get_model(ocl),"")!=0){
		if((retVal=OCl_load_model(ocl,TRUE))!=RETURN_OK){
			print_error(OCL_get_response_error(ocl),OCL_error_handling(retVal),FALSE);
			OCl_set_model(ocl, "");
		}
	}
	rl_getc_function=readline_input;
	char *messagePrompted=NULL;
	do{
		exitProgram=canceled=FALSE;
		free(messagePrompted);
		printf("%s\n",promptFont);
		messagePrompted=readline_get(PROMPT_DEFAULT, FALSE);
		if(exitProgram){
			printf("\n⎋");
			break;
		}
		if(canceled || strcmp(messagePrompted,"")==0) continue;
		printf("↵\n");
		if(strcmp(messagePrompted,"flush;")==0){
			OCl_flush_context();
			continue;
		}
		if(strcmp(messagePrompted,"models;")==0){
			char **models=NULL;
			int cantModels=OCl_get_models(ocl, &models);
			if(cantModels<0) {
				print_error(OCL_get_response_error(ocl),OCL_error_handling(cantModels),FALSE);
				printf("\n");
				continue;
			}
			printf("%s\n", responseFont);
			for(int i=0;i<cantModels;i++){
				printf("%s\n", models[i]);
				free(models[i]);
			}
			free(models);
			continue;
		}
		if(strncmp(messagePrompted,"model;", strlen("model;"))==0){
			if(strcmp(OCl_get_model(ocl),"")!=0){
				if((retVal=OCl_load_model(ocl,FALSE))!=RETURN_OK){
					print_error(OCL_get_response_error(ocl),OCL_error_handling(retVal),FALSE);
					printf("\n");
				}
			}
			if(strcmp(messagePrompted+strlen("model;"),"")==0){
				OCl_set_model(ocl,model);
			}else{
				OCl_set_model(ocl,messagePrompted+strlen("model;"));
			}
			if((retVal=OCl_load_model(ocl,TRUE))!=RETURN_OK){
				print_error(OCL_get_response_error(ocl),OCL_error_handling(retVal),FALSE);
				printf("\n");
				OCl_set_model(ocl, "");
				continue;
			}
			continue;
		}
		if(strncmp(messagePrompted,"roles;", strlen("roles;"))==0){
			//TODO
			continue;
		}
		if(strncmp(messagePrompted,"role;", strlen("role;"))==0){
			if(strcmp(messagePrompted+strlen("role;"),"")==0){
				OCl_set_role(ocl, systemRole);
				continue;
			}
			if(rolesFile==NULL){
				print_error("No role file provided\n", "", FALSE);
				continue;
			}
			FILE *f=fopen(rolesFile,"r");
			if(f==NULL){
				perror("");
				exit(1);
			}
			ssize_t chars=0;
			size_t len=0;
			char *line=NULL;
			Bool found=FALSE;
			char role[255]="";
			snprintf(role, 255,"[%s]", messagePrompted+strlen("role;"));
			char *newSystemRole=NULL;
			newSystemRole=malloc(1);
			newSystemRole[0]=0;
			while((chars=getline(&line, &len, f))!=-1){
				if(strncmp(line, role, strlen(role))==0){
					found=TRUE;
					while((chars=getline(&line, &len, f))!=-1){
						if(line[0]=='[') break;
						newSystemRole=realloc(newSystemRole,strlen(newSystemRole)+chars+1);
						strcat(newSystemRole,line);
					}
				}
				if(found) break;
			}
			(found)?(OCl_set_role(ocl, newSystemRole)):(print_error("Role No Found\n", "", FALSE));
			fclose(f);
			free(newSystemRole);
			continue;
		}
		printf("\n");
		if((retVal=OCl_send_chat(ocl,messagePrompted))!=RETURN_OK){
			switch(retVal){
			case OCL_ERR_RESPONSE_MESSAGE_ERROR:
				print_error(OCL_get_response_error(ocl),"",FALSE);
				break;
			default:
				if(canceled){
					printf("\n");
					break;
				}
				print_error("",OCL_error_handling(retVal),FALSE);
				break;
			}
		}else{
			if(showResponseInfo && !canceled) print_response_info();
		}
		add_history(messagePrompted);
		printf("\n");
	}while(TRUE);
	free(messagePrompted);
	close_program(ocl);
}

