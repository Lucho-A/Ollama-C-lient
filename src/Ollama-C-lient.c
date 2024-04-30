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

#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <errno.h>
#include "../src/lib/libOllama-C-lient.h"

#define PROGRAM_NAME					"Ollama-C-lient"
#define PROGRAM_VERSION					"0.0.1"

#define PROMPT_DEFAULT					"-> "
#define BANNER 							printf("\n%s%s v%s by L. <https://github.com/lucho-a/ollama-c-lient>%s\n\n",Colors.h_white,PROGRAM_NAME, PROGRAM_VERSION,Colors.def);

bool canceled=FALSE, exitProgram=FALSE;
int prevInput=0;
struct Colors Colors;
OCl *ocl=NULL;

static int close_program(OCl *ocl){
	OCl_load_model(ocl,FALSE);
	OCl_free(ocl);
	rl_clear_history();
	printf("%s\n",Colors.def);
	exit(EXIT_SUCCESS);
}

static void print_error(char *msg, char *error, bool exitProgram){
	printf("%s%s%s%s",Colors.h_red, msg,error,Colors.def);
	if(exitProgram){
		printf("\n\n");
		exit(EXIT_FAILURE);
	}
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

static void signal_handler(int signalType){
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
	OCl_init();
	OCl_init_colors(FALSE);
	ocl=OCl_get_instance();
	int retVal=0;
	for(int i=1;i<argc;i++){
		if(strcmp(argv[i],"--version")==0){
			BANNER;
			exit(EXIT_SUCCESS);
		}
		if(strcmp(argv[i],"--server-addr")==0){
			if((retVal=OCl_set_server_addr(ocl, argv[i+1]))!=RETURN_OK) print_error("\n",OCL_error_handling(retVal),TRUE);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--server-port")==0){
			if((retVal=OCl_set_server_port(ocl, argv[i+1]))!=RETURN_OK) print_error("\n",OCL_error_handling(retVal),TRUE);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--model-file")==0){
			if(argv[i+1]==NULL) print_error("\n",OCL_error_handling(ERR_MODEL_FILE_NOT_FOUND),TRUE);
			FILE *f=fopen(argv[i+1],"r");
			if(f==NULL)print_error("\n",OCL_error_handling(ERR_MODEL_FILE_NOT_FOUND),TRUE);
			modelFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--context-file")==0){
			if((retVal=OCl_set_contextfile(ocl, argv[i+1]))!=RETURN_OK) print_error("\n",OCL_error_handling(retVal),TRUE);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--show-response-info")==0){
			OCl_set_show_resp_info(ocl, TRUE);
			continue;
		}
		if(strcmp(argv[i],"--uncolored")==0){
			OCl_init_colors(TRUE);
			continue;
		}
		printf("\n");
		print_error(argv[i],": argument not recognized",TRUE);
	}
	printf("\n");
	if((retVal=OCl_load_modelfile(ocl, modelFile))!=RETURN_OK) print_error("",OCL_error_handling(retVal),TRUE);
	if((retVal=OCl_import_context(ocl))!=RETURN_OK) print_error("",OCL_error_handling(retVal),TRUE);
	if((retVal=OCl_check_service_status(ocl))!=RETURN_OK) print_error("",OCL_error_handling(retVal),TRUE);
	if((retVal=OCl_load_model(ocl,TRUE))!=RETURN_OK) print_error("\n\n",OCL_error_handling(retVal),TRUE);
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
		printf("↵\n\n%s",Colors.def);
		if(strcmp(messagePrompted,"flush;")==0){
			OCl_flush_context();
			continue;
		}
		if((retVal=OCl_send_chat(ocl,messagePrompted))!=RETURN_OK){
			switch(retVal){
			case ERR_RESPONSE_MESSAGE_ERROR:
				break;
			default:
				if(canceled){
					printf("\n\n");
					break;
				}
				print_error("",OCL_error_handling(retVal),FALSE);
				break;
			}
		}
		add_history(messagePrompted);
		printf("\n");
	}while(TRUE);
	free(messagePrompted);
	close_program(ocl);
}
