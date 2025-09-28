/*
 ============================================================================
 Name        : Ollama-C-lient.c
 Author      : L. (lucho-a.github.io)
 Copyright   : GNU General Public License v3.0
 Description : Main file
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "lib/libOllama-C-lient.h"

#define PROGRAM_NAME					"Ollama-C-lient"
#define PROGRAM_VERSION					"0.0.4"

#define BANNER 							printf("\n%s v%s by L. <https://github.com/lucho-a/ollama-c-lient>\n\n",PROGRAM_NAME, PROGRAM_VERSION);

#define	RESPONSE_SPEED					0
#define	MIN_STDOUT_BUFFER_SIZE			0

OCl *ocl=NULL;

enum msgTypes{
	ERROR_MSG=0,
	SYSTEM_MSG,
	INFO_MSG
};

struct OclParams{
	char serverAddr[16];
	char serverPort[6];
	char socketConnTo[8];
	char socketSendTo[8];
	char socketRecvTo[8];
	char apiKey[1024];
	char model[128];
	bool noThink;
	char temp[8];
	char repeat_last_n[8];
	char repeat_penalty[8];
	char top_k[8];
	char top_p[8];
	char min_p[8];
	char seed[8];
	char keepalive[8];
	char maxMsgCtx[8];
	char maxTokensCtx[8];
	char *systemRole;
	char *systemRoleFile;
	char *contextFile;
	char *staticContextFile;
	char *toolsFile;
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
	bool executeTools;
	bool showThoughts;
	bool showResponseInfo;
	bool showModels;
	bool stdoutParsed;
	bool stdoutChunked;
	int stdoutBufferSize;
	bool stdoutJson;
	struct Colors colors;
	char charsToExclude[1024];
};

struct SendingMessage{
	char *input;
	char *imageFile;
};

struct ProgramOpts po={0};
struct SendingMessage sm={0};
bool thinking=false;
char chunkings[8196]="";
int toolsRecv=0;
char *toolsResponses[128]={NULL};

static void show_help(char *programName){
	BANNER
	printf("USAGE: %s [OPTIONS]\n", programName);
	printf("\nOptions:\n\n");
	printf("--version \t\t\t\t\t\t shows version.\n");
	printf("--help \t\t\t\t\t\t\t shows this.\n");
	printf("--server-addr \t\t\t string:'127.0.0.1' \t URL or IP of the server.\n");
	printf("--server-port \t\t\t int:443 [1-65535] \t listening port. Must be SSL/TLS.\n");
	printf("--response-speed \t\t int:0 [>=0] \t\t in microseconds, if > 0, the responses will be sending out to stdout at the interval set up.\n");
	printf("--socket-conn-to \t\t int:5 [>=0] \t\t in seconds, sets up the connection time out.\n");
	printf("--socket-send-to \t\t int:5 [>=0] \t\t in seconds, sets up the sending time out.\n");
	printf("--socket-recv-to \t\t int:15 [>=0] \t\t in seconds, sets up the receiving time out.\n");
	printf("--api-key \t\t\t string:NULL \t\t sets the API key.\n");
	printf("--model \t\t\t string:NULL \t\t model to use.\n");
	printf("--no-think \t\t\t N/A:false \t\t sets a no-thinking status for the model.\n");
	printf("--temperature \t\t\t double:0.5 [>=0] \t sets the temperature parameter.\n");
	printf("--seed \t\t\t\t int:0 [>=0] \t\t sets the seed parameter.\n");
	printf("--repeat-last-n \t\t int:64 [>=-1] \t\t sets the repeat_last_n parameter.\n");
	printf("--repeat-penalty \t\t double:1.1 [>=0] \t sets the repeat_penalty parameter.\n");
	printf("--top-k \t\t\t int:40 [>=0] \t\t sets the top_k parameter.\n");
	printf("--top-p \t\t\t double:0.9 [>=0] \t sets the top_p parameter.\n");
	printf("--min-p \t\t\t double:0.0 [>=0] \t sets the min_p parameter.\n");
	printf("--keep-alive \t\t\t int:300 [>=0] \t\t in seconds, tell to the server how many seconds the model will be available until unloaded.\n");
	printf("--max-msgs-ctx \t\t\t int:3 [>=0] \t\t sets the maximum messages to be added as context in the messages.\n");
	printf("--max-msgs-tokens \t\t int:4096 [>=0] \t sets the maximum tokens.\n");
	printf("--system-role \t\t\t string:'' \t\t sets the system role. Override '--system-role-file'.\n");
	printf("--system-role-file \t\t string:NULL \t\t sets the path to the file that include the system role.\n");
	printf("--context-file \t\t\t string:NULL \t\t file where the interactions (except the queries ended with ';') will be stored.\n");
	printf("--static-context-file \t\t string:NULL \t\t file where the interactions included into it (separated by '\\t') will be include (statically) as interactions in every query.\n");
	printf("--tools-file \t\t\t string:NULL \t\t file where the tools to be incorporated to the interactions are included.\n");
	printf("--image-file \t\t\t string:NULL \t\t Image file to attach to the query.\n");
	printf("--color-font-response \t\t string:'00;00;00' \t in ANSI format, set the color used for responses.\n");
	printf("--color-font-system \t\t string:'00;00;00' \t in ANSI format, set the color used for program's messages.\n");
	printf("--color-font-info \t\t string:'00;00;00' \t in ANSI format, set the color used for response's info ('--show-response-info').\n");
	printf("--color-font-error \t\t string:'00;00;00' \t in ANSI format, set the color used for errors.\n");
	printf("--show-response-info \t\t N/A:false \t\t shows the responses' information, as tokens count, duration, etc.\n");
	printf("--show-thoughts \t\t N/A:false \t\t shows what the model is 'thinking' in reasoning models like 'deepseek-r1'.\n");
	printf("--show-models \t\t\t N/A:false \t\t shows the models available.\n");
	printf("--stdout-parsed \t\t N/A:false \t\t parses the output (useful for speeching/chatting).\n");
	printf("--stdout-chunked \t\t N/A:false \t\t chunks the output by paragraph (particularly useful for speeching). Sets '--stdout-parsed', as well. \n");
	printf("--stdout-buffer-size \t\t int:0 \t\t Set the minimum char length of the stream before starting stdout. \n");
	printf("--exclude-chars \t\t string:NULL \t\t Chars to exclude from the responses. \n");
	printf("--stdout-json \t\t\t N/A:false \t\t writes stdout in JSON format. Output always no streamed and in RAW format.\n");
	printf("--execute-tools \t\t N/A:false \t\t execute the tools (function) with the arguments.\n\n");
	printf("Example: \n\n");
	printf("$ (echo 'What can you tell me about my storage: ' && df) | ./ollama-c-lient --model deepseek-r1 --stdout-parsed --response-speed 1\n");
	printf("\nSee https://github.com/lucho-a/ollama-c-lient for a full description & more examples.\n\n");
}

static int close_program(bool finishWithErrors){
	oclCanceled=true;
	if(ocl) OCl_free(ocl);
	OCl_shutdown();
	free(po.ocl.staticContextFile);
	po.ocl.staticContextFile=NULL;
	free(po.ocl.contextFile);
	po.ocl.contextFile=NULL;
	free(po.ocl.systemRole);
	po.ocl.systemRole=NULL;
	free(po.ocl.systemRoleFile);
	po.ocl.systemRoleFile=NULL;
	free(po.ocl.toolsFile);
	po.ocl.toolsFile=NULL;
	free(sm.input);
	sm.input=NULL;
	for(int i=0;i<toolsRecv;i++) free(toolsResponses[i]);
	if(isatty(fileno(stdout))) fputs("\x1b[0m",stdout);
	fflush(stdout);
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

static void print_msg_to_stderr(char *msg, char *extraMsg, bool exitProgram, int msgType){
	char sdterrMsg[1024]="";
	switch(msgType){
	case ERROR_MSG:
		if(isatty(fileno(stderr))){
			snprintf(sdterrMsg,1024,"%sERROR: %s %s\x1b[0m\n",po.colors.colorFontError, msg,extraMsg);
		}else{
			snprintf(sdterrMsg,1024,"ERROR: %s %s\n",msg,extraMsg);
		}
		break;
	case SYSTEM_MSG:
		if(isatty(fileno(stderr))){
			snprintf(sdterrMsg,1024,"%sSYSTEM: %s\x1b[0m\n",po.colors.colorFontSystem, msg);
		}else{
			snprintf(sdterrMsg,1024,"SYSTEM: %s\n",msg);
		}
		break;
	case INFO_MSG:
		if(isatty(fileno(stderr))){
			snprintf(sdterrMsg,1024,"%s%s\x1b[0m\n",po.colors.colorFontInfo, msg);
		}else{
			snprintf(sdterrMsg,1024,"%s\n",msg);
		}
		break;
	default:
		break;
	}
	for(size_t i=0;i<strlen(sdterrMsg);i++){
		fputc(sdterrMsg[i],stderr);
		fflush(stderr);
		usleep(po.responseSpeed);
	}
	if(exitProgram) close_program(true);
}

static void print_response_info(){
	char buffer[1024]="";
	snprintf(buffer,1024,"\n\n- Time spent loading the model (load_duration): %.4fs",OCL_get_response_load_duration(ocl));
	print_msg_to_stderr(buffer,"",false,INFO_MSG);
	snprintf(buffer,1024,"- Time spent evaluating the prompt (prompt_eval_duration): %.4fs",OCL_get_response_prompt_eval_duration(ocl));
	print_msg_to_stderr(buffer,"",false,INFO_MSG);
	snprintf(buffer,1024,"- Time spent evaluating the response (eval_duration): %.4fs",OCL_get_response_eval_duration(ocl));
	print_msg_to_stderr(buffer,"",false,INFO_MSG);
	snprintf(buffer,1024,"- Time spent generating the response (total_duration): %.4fs",OCL_get_response_total_duration(ocl));
	print_msg_to_stderr(buffer,"",false,INFO_MSG);
	snprintf(buffer,1024,"- Number of tokens in the prompt (prompt_eval_count): %d",OCL_get_response_prompt_eval_count(ocl));
	print_msg_to_stderr(buffer,"",false,INFO_MSG);
	snprintf(buffer,1024,"- Number of tokens in the response (eval_count): %d",OCL_get_response_eval_count(ocl));
	print_msg_to_stderr(buffer,"",false,INFO_MSG);
	snprintf(buffer,1024,"- Tokens per sec.: %.4f",OCL_get_response_tokens_per_sec(ocl));
	print_msg_to_stderr(buffer,"",false,INFO_MSG);
	snprintf(buffer,1024,"- Characters in content: %d",OCL_get_response_chars_content(ocl));
	print_msg_to_stderr(buffer,"",false,INFO_MSG);
	snprintf(buffer,1024,"- Response size: %.2f kb",OCL_get_response_size(ocl)/1024.0);
}

char *parse_output(const char *in, bool parse, bool removeChars){
	char buffer[5]="";
	char *buff=malloc(strlen(in)+1);
	memset(buff,0,strlen(in)+1);
	int cont=0;
	for(size_t i=0;i<strlen(in);i++){
		if(parse){
			if(in[i]=='\\'){
				switch(in[i+1]){
				case 'n':
					buff[cont++]='\n';
					break;
				case 'r':
					buff[cont++]='\r';
					break;
				case 't':
					buff[cont++]='\r';
					break;
				case '\\':
					buff[cont++]='\\';
					break;
				case '"':
					buff[cont++]='\"';
					break;
				case 'u':
					snprintf(buffer,5,"%c%c%c%c",in[i+2],in[i+3],in[i+4],in[i+5]);
					buff[cont++]=(int)strtol(buffer,NULL,16);
					i+=4;
					break;
				default:
					break;
				}
				i++;
				continue;
			}
		}
		if(removeChars){
			bool ok=true;
			for(size_t j=0;j<strlen(po.charsToExclude);j++){
				if(in[i]==po.charsToExclude[j]){
					ok=false;
					break;
				}
			}
			if(ok) buff[cont++]=in[i];
			continue;
		}
		buff[cont++]=in[i];
	}
	buff[cont]=0;
	return buff;
}

static void print_response(char const *token, bool done, int responseType){
	if(responseType==OCL_TOOL_TYPE){
		if(po.executeTools){
			char cmd[1024]="", args[1024]="";
			char const *contentCmd=strstr(token, "\"name\":");
			if(contentCmd!=NULL){
				size_t i=0, len=strlen("\"name\":");
				for(i=len+1;(contentCmd[i-1]=='\\' || contentCmd[i]!='"');i++) cmd[i-len-1]=contentCmd[i];
				cmd[i-len-1]=0;
			}
			char const *contentArg=strstr(token, "\"arguments\":{");
			if(contentArg!=NULL){
				size_t i=0, len=strlen("\"arguments\":{");
				for(i=len;(contentArg[i-1]=='\\' || contentArg[i]!='}');i++) args[i-len]=contentArg[i];
				args[i-len]=0;
			}
			int argsLen=strlen(args);
			bool semiColonFound=false;
			char argums[1024][1024]={""};
			int contArgs=0;
			for(int i=0; i<argsLen;i++){
				if(args[i]==':') semiColonFound=true;
				if(semiColonFound){
					int cont=0;
					for(int j=i+1;args[j]!=',' && j<argsLen; j++){
						argums[contArgs][cont++]=args[j];
					}
					semiColonFound=false;
					contArgs++;
				}
			}
			for(int i=0;i<contArgs;i++){
				strcat(cmd," ");
				strcat(cmd, argums[i]);
			}
			FILE *fp=popen(cmd, "r");
			char c=0;
			int cont=0, initialBuffer=1024*10;
			toolsResponses[toolsRecv]=malloc(initialBuffer);
			memset(toolsResponses[toolsRecv],0,initialBuffer);
			while ((c=fgetc(fp)) != EOF && !oclCanceled) {
				if(cont>=initialBuffer) toolsResponses[toolsRecv]=realloc(toolsResponses[toolsRecv],initialBuffer*=2);
				if(!po.stdoutJson){
					usleep(po.responseSpeed);
					fputc(c, stdout);
					fflush(stdout);
				}
				toolsResponses[toolsRecv][cont++]=c;
			}
			toolsRecv++;
			pclose(fp);
			return;
		}
		if(!po.stdoutJson){
			fputs(token, stdout);
			fflush(stdout);
		}
		return;
	}
	if(responseType==OCL_THINKING_TYPE && !thinking && !po.stdoutJson){
		thinking=true;
		fputs("(Thinking...)\n", stdout);
		fflush(stdout);
	}
	if(responseType!=OCL_THINKING_TYPE && thinking && !po.stdoutJson){
		thinking=false;
		fputs("(Stop thinking...)\n", stdout);
		fflush(stdout);
	}
	if(po.stdoutChunked && !po.stdoutJson){
		if(responseType==OCL_THINKING_TYPE && !po.showThoughts) return;
		strncat(chunkings,token,8196-1);
		if((strstr(token, "\\n") && ((int) strlen(chunkings))>po.stdoutBufferSize) || done){
			char *parsedOut=parse_output(chunkings, true, true);
			fputs(parsedOut, stdout);
			fflush(stdout);
			memset(chunkings,0,8196);
			free(parsedOut);
			parsedOut=NULL;
		}
		return;
	}
	if(po.responseSpeed==0) return;
	if(responseType==OCL_THINKING_TYPE && !po.showThoughts) return;
	strncat(chunkings,token,8196-1);
	if((int)strlen(chunkings)>po.stdoutBufferSize || done){
		if(po.stdoutParsed){
			char *parsedOut=parse_output(chunkings, true, true);
			for(size_t i=0;i<strlen(parsedOut) && !oclCanceled;i++){
				usleep(po.responseSpeed);
				fputc(parsedOut[i], stdout);
				fflush(stdout);
			}
			free(parsedOut);
			parsedOut=NULL;

		}else{
			char *parsedOut=parse_output(chunkings, false, true);
			for(size_t i=0;i<strlen(parsedOut) && !oclCanceled;i++){
				usleep(po.responseSpeed);
				fputc(parsedOut[i], stdout);
				fflush(stdout);
			}
			free(parsedOut);
			parsedOut=NULL;
		}
		memset(chunkings,0,8196);
	}
}

void create_json(){
	char *jsonTemplate=NULL;
	char *inParsed=NULL;
	OCl_parse_string(&inParsed, sm.input);
	char *thoughts=OCL_get_response_thoughts(ocl);
	char *response=OCL_get_response(ocl);
	char **tools=NULL;
	int cTools=OCL_get_response_tools(ocl, &tools);
	char toolsTemplate[1024]="";
	char toolsRecvTemplate[1024*10]="";
	for(int i=0;i<cTools;i++){
		strcat(toolsTemplate, "[");
		strcat(toolsTemplate, tools[i]);
		strcat(toolsTemplate, "],");
		char *toolResultParsed=NULL;
		OCl_parse_string(&toolResultParsed, toolsResponses[i]);
		strcat(toolsRecvTemplate, "[\"");
		strcat(toolsRecvTemplate, toolResultParsed);
		strcat(toolsRecvTemplate, "\"],");
		free(toolResultParsed);
		toolResultParsed=NULL;
	}
	toolsTemplate[strlen(toolsTemplate)-1]=0;
	toolsRecvTemplate[strlen(toolsRecvTemplate)-1]=0;
	size_t len=strlen(thoughts)+strlen(response)+strlen(inParsed)+strlen(toolsTemplate)+strlen(toolsRecvTemplate)+2048;
	jsonTemplate=malloc(len);
	memset(jsonTemplate,0,len);
	time_t timestamp = time(NULL);
	struct tm tm = *localtime(&timestamp);
	char strTimeStamp[50]="";
	snprintf(strTimeStamp,sizeof(strTimeStamp)
			,"%d%02d%02d_%02d%02d%02d_UTC%s"
			,tm.tm_year + 1900
			,tm.tm_mon + 1
			,tm.tm_mday
			,tm.tm_hour
			,tm.tm_min
			,tm.tm_sec
			,tm.tm_zone);
	snprintf(jsonTemplate,len,
			"{\n"
			"\"status\": \"success\",\n"
			"\"model\": \"%s\",\n"
			"\"prompt\": \"%s\",\n"
			"\"thoughts\": \"%s\",\n"
			"\"response\": \"%s\",\n"
			"\"tools\": [%s],\n"
			"\"tool_response\": [%s],\n"
			"\"timestamp\": \"%s\",\n"
			"\"load_duration\": %.4f,\n"
			"\"prompt_eval_duration\": %.4f,\n"
			"\"eval_duration\": %.4f,\n"
			"\"total_duration\": %.4f,\n"
			"\"prompt_eval_count\": %d,\n"
			"\"eval_count\": %d,\n"
			"\"tokens_per_sec\": %.4f,\n"
			"\"count_chars\": %d,\n"
			"\"response_size\": %.2f\n"
			"}\n"
			,po.ocl.model
			,inParsed
			,thoughts
			,response
			,toolsTemplate
			,toolsRecvTemplate
					,strTimeStamp
					,OCL_get_response_load_duration(ocl)
					,OCL_get_response_prompt_eval_duration(ocl)
					,OCL_get_response_eval_duration(ocl)
					,OCL_get_response_total_duration(ocl)
					,OCL_get_response_prompt_eval_count(ocl)
					,OCL_get_response_eval_count(ocl)
					,OCL_get_response_tokens_per_sec(ocl)
					,OCL_get_response_chars_content(ocl)
					,OCL_get_response_size(ocl)/1024.0
	);
	fputs(jsonTemplate, stdout);
	fflush(stdout);
	for(int i=0;i<cTools;i++) free(tools[i]);
	free(tools);
	free(jsonTemplate);
	free(inParsed);
}

void *start_sending_message(void *arg){
	struct SendingMessage *sm=arg;
	int retVal=OCl_send_chat(ocl,sm->input,sm->imageFile ,print_response);
	if(retVal!=OCL_RETURN_OK){
		if(oclCanceled) printf("\n");
		oclCanceled=true;
		print_msg_to_stderr(OCL_error_handling(ocl, retVal),"",false, ERROR_MSG);
		pthread_exit("-1");
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	signal(SIGINT, signal_handler);
	signal(SIGPIPE, signal_handler);
	signal(SIGTSTP, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGSEGV, signal_handler);
	po.responseSpeed=RESPONSE_SPEED;
	po.stdoutBufferSize=MIN_STDOUT_BUFFER_SIZE;
	snprintf(po.colors.colorFontResponse,16,"\x1b[0m");
	snprintf(po.colors.colorFontError,16,"\x1b[0m");
	snprintf(po.colors.colorFontSystem,16,"\x1b[0m");
	snprintf(po.colors.colorFontInfo,16,"\x1b[0m");
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
		line=NULL;
	}
	int retVal=0;
	if((retVal=OCl_init())!=OCL_RETURN_OK)
		print_msg_to_stderr("OCl Init error. ", OCL_error_handling(ocl,retVal),true, ERROR_MSG);
	for(int i=1;i<argc;i++){
		if(strcmp(argv[i],"--version")==0){
			BANNER
			close_program(false);
		}
		if(strcmp(argv[i],"--help")==0){
			show_help(argv[0]);
			close_program(false);
		}
		if(strcmp(argv[i],"--server-addr")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.serverAddr,16,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--server-port")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.serverPort,6,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--socket-conn-to")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.socketConnTo,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--socket-send-to")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.socketSendTo,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--socket-recv-to")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.socketRecvTo,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--response-speed")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			char *tail=NULL;
			po.responseSpeed=strtol(argv[i+1], &tail, 10);
			if(po.responseSpeed<0 || tail[0]!=0){
				po.responseSpeed=RESPONSE_SPEED;
				print_msg_to_stderr("Response speed not valid.","",true, ERROR_MSG);
			}
			i++;
			continue;
		}
		if(strcmp(argv[i],"--api-key")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.apiKey,1024,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--model")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.model,128,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--no-think")==0){
			po.ocl.noThink=true;
			continue;
		}
		if(strcmp(argv[i],"--temperature")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.temp,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--seed")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.seed,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--repeat-last-n")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.repeat_last_n,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--repeat-penalty")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.repeat_penalty,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--top-k")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.top_k,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--top-p")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.top_p,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--min-p")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.min_p,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--keep-alive")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.keepalive,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--max-msgs-ctx")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.maxMsgCtx,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--max-msgs-tokens")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.ocl.maxTokensCtx,8,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--system-role")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			free(po.ocl.systemRole);
			po.ocl.systemRole=NULL;
			po.ocl.systemRole=malloc(strlen(argv[i+1])+1);
			memset(po.ocl.systemRole,0,strlen(argv[i+1])+1);
			snprintf(po.ocl.systemRole,strlen(argv[i+1])+1,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--system-role-file")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			po.ocl.systemRoleFile=malloc(strlen(argv[i+1])+1);
			memset(po.ocl.systemRoleFile,0,strlen(argv[i+1])+1);
			snprintf(po.ocl.systemRoleFile,strlen(argv[i+1])+1,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--image-file")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			sm.imageFile=argv[i+1];
			i++;
			continue;
		}
		if(strcmp(argv[i],"--static-context-file")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			po.ocl.staticContextFile=malloc(strlen(argv[i+1])+1);
			memset(po.ocl.staticContextFile,0,strlen(argv[i+1])+1);
			snprintf(po.ocl.staticContextFile,strlen(argv[i+1])+1,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--context-file")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			po.ocl.contextFile=malloc(strlen(argv[i+1])+1);
			memset(po.ocl.contextFile,0,strlen(argv[i+1])+1);
			snprintf(po.ocl.contextFile,strlen(argv[i+1])+1,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--tools-file")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			po.ocl.toolsFile=malloc(strlen(argv[i+1])+1);
			memset(po.ocl.toolsFile,0,strlen(argv[i+1])+1);
			snprintf(po.ocl.toolsFile,strlen(argv[i+1])+1,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--color-font-error")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			int f1,f2,color;
			int retVal=sscanf(argv[i+1], "%d;%d;%d", &f1, &f2, &color);
			if(retVal!=3) print_msg_to_stderr("--color-font-error: argument not valid: ",argv[i+1],true, ERROR_MSG);
			snprintf(po.colors.colorFontError,16,"\e[%d;%d;%dm",f1, f2, color);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--color-font-response")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			int f1,f2,color;
			int retVal=sscanf(argv[i+1], "%d;%d;%d", &f1, &f2, &color);
			if(retVal!=3) print_msg_to_stderr("--color-font-response: argument not valid: ",argv[i+1],true, ERROR_MSG);
			snprintf(po.colors.colorFontResponse,16,"\e[%d;%d;%dm",f1, f2, color);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--color-font-system")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			int f1,f2,color;
			int retVal=sscanf(argv[i+1], "%d;%d;%d", &f1, &f2, &color);
			if(retVal!=3) print_msg_to_stderr("--color-font-system: argument not valid: ",argv[i+1],true, ERROR_MSG);
			snprintf(po.colors.colorFontSystem,16,"\e[%d;%d;%dm",f1, f2, color);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--color-font-info")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			int f1,f2,color;
			int retVal=sscanf(argv[i+1], "%d;%d;%d", &f1, &f2, &color);
			if(retVal!=3) print_msg_to_stderr("--color-font-info: argument not valid: ",argv[i+1],true, ERROR_MSG);
			snprintf(po.colors.colorFontInfo,16,"\e[%d;%d;%dm",f1, f2, color);
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
		if(strcmp(argv[i],"--stdout-buffer-size")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			char *tail=NULL;
			po.stdoutBufferSize=strtol(argv[i+1], &tail, 10);
			if(po.stdoutBufferSize<0 || tail[0]!=0){
				po.stdoutBufferSize=MIN_STDOUT_BUFFER_SIZE;
				print_msg_to_stderr("Min. stdout buffer size not valid.","",true, ERROR_MSG);
			}
			i++;
			continue;
		}
		if(strcmp(argv[i],"--exclude-chars")==0){
			if(!argv[i+1]) print_msg_to_stderr("Argument missing: ",argv[i],true, ERROR_MSG);
			snprintf(po.charsToExclude,1024,"%s",argv[i+1]);
			i++;
			continue;
		}
		if(strcmp(argv[i],"--stdout-json")==0){
			po.stdoutJson=true;
			continue;
		}
		if(strcmp(argv[i],"--show-models")==0){
			po.showModels=true;
			continue;
		}
		if(strcmp(argv[i],"--execute-tools")==0){
			po.executeTools=true;
			continue;
		}
		print_msg_to_stderr(argv[i],": argument not recognized",true, ERROR_MSG);
	}
	if((retVal=OCl_get_instance(
			&ocl,
			po.ocl.serverAddr,
			po.ocl.serverPort,
			po.ocl.socketConnTo,
			po.ocl.socketSendTo,
			po.ocl.socketRecvTo,
			po.ocl.apiKey,
			po.ocl.model,
			po.ocl.noThink,
			po.ocl.keepalive,
			po.ocl.systemRole,
			po.ocl.systemRoleFile,
			po.ocl.maxMsgCtx,
			po.ocl.temp,
			po.ocl.repeat_last_n,
			po.ocl.repeat_penalty,
			po.ocl.seed,
			po.ocl.top_k,
			po.ocl.top_p,
			po.ocl.min_p,
			po.ocl.maxTokensCtx,
			po.ocl.contextFile,
			po.ocl.staticContextFile,
			po.ocl.toolsFile))!=OCL_RETURN_OK)
		print_msg_to_stderr("",OCL_error_handling(ocl,retVal),true, ERROR_MSG);
	if(isatty(fileno(stdout))) printf("%s",po.colors.colorFontResponse);
	if(po.showModels){
		char models[512][512]={""};
		int cantModels=OCl_get_models(ocl, models);
		if(cantModels<0) print_msg_to_stderr(OCL_error_handling(ocl,cantModels),"",true, ERROR_MSG);
		for(int i=0;i<cantModels;i++){
			printf("  - ");
			for(size_t j=0;j<strlen(models[i]);j++){
				usleep(po.responseSpeed);
				fputc(models[i][j], stdout);
				fflush(stdout);
			}
			printf("\n");
		}
	}
	if(po.stdoutJson) po.responseSpeed=0;
	if(po.stdoutChunked) po.stdoutParsed=true;
	if(sm.input){
		pthread_t tSendingMessage;
		void *tRetVal=NULL;
		pthread_create(&tSendingMessage, NULL, start_sending_message, &sm);
		pthread_join(tSendingMessage,&tRetVal);
		if(tRetVal!=NULL) close_program(true);
		if(po.responseSpeed==0 && !oclCanceled){
			if(po.stdoutJson){
				create_json();
			}else{
				if(!po.stdoutParsed){
					if(po.showThoughts){
						fputs("<thinking>", stdout);
						fputs(OCL_get_response_thoughts(ocl), stdout);
						fputs("</thinking>\n", stdout);
					}
					fputs(OCL_get_response(ocl), stdout);
					fflush(stdout);
				}else{
					if(!po.stdoutChunked){
						char *out=NULL;
						if(po.showThoughts){
							fputs("<thinking>", stdout);
							out=parse_output(OCL_get_response_thoughts(ocl),false, true);
							fputs(out, stdout);
							fputs("</thinking>\n", stdout);
						}
						out=parse_output(OCL_get_response(ocl),false, true);
						fputs(out, stdout);
						fflush(stdout);
						free(out);
						out=NULL;
					}
				}
			}
		}
		if(po.showResponseInfo && !po.stdoutJson && !oclCanceled) print_response_info();
		printf("\n");
	}
	close_program(false);
}
