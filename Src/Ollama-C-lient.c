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

bool canceled=FALSE, exitProgram=FALSE;
int prevInput=0;
struct Colors Colors;
OCl *ocl=NULL;

int close_program(OCl *ocl){
	OCl_load_model(ocl,FALSE);
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
		close_program(ocl);
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
	ocl=OCl_init();
	init_colors(FALSE);
	for(int i=1;i<argc;i++){
		if(strcmp(argv[i],"--version")==0){
			BANNER;
			exit(EXIT_SUCCESS);
		}
		if(strcmp(argv[i],"--server-addr")==0){
			ocl->srvAddr=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--server-port")==0){
			ocl->srvPort=strtol(argv[i+1],NULL,10);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--model-file")==0){
			modelFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--context-file")==0){
			if(argv[i+1]==NULL) print_error("Error opening context file: ",strerror(errno),TRUE);
			ocl->contextFile=argv[i+1];
			FILE *f=fopen(ocl->contextFile,"a");
			if(f==NULL) print_error("Error opening context file: ",strerror(errno),TRUE);
			fclose(f);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--show-response-info")==0){
			ocl->showResponseInfo=TRUE;
			continue;
		}
		print_error(argv[i],": argument not recognized",TRUE);
	}
	int retVal=0;
	if((retVal=OCl_load_modelfile(ocl, modelFile))!=RETURN_OK) print_error("",error_handling(retVal),TRUE);
	if((retVal=OCl_import_context(ocl))!=RETURN_OK) print_error("",error_handling(retVal),TRUE);
	if((retVal=OCl_check_service_status(ocl))!=RETURN_OK) print_error("",error_handling(retVal),TRUE);
	if((retVal=OCl_load_model(ocl,TRUE))!=RETURN_OK) print_error("",error_handling(retVal),TRUE);
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
		printf("↵\n%s",Colors.def);
		if((retVal=OCl_send_chat(ocl,messagePrompted))!=RETURN_OK){
			switch(retVal){
			case ERR_RESPONSE_MESSAGE_ERROR:
				break;
			default:
				if(canceled){
					printf("\n");
					break;
				}
				print_error("",error_handling(retVal),FALSE);
				break;
			}
		}
		add_history(messagePrompted);
	}while(TRUE);
	free(messagePrompted);
	close_program(ocl);
}
