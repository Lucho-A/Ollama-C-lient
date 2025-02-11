# Ollama-C-lient

C program for interacting with [Ollama](https://github.com/ollama/ollama) server from a Linux terminal. It is not meant to be a complete library at all. At the moment, it's just the simplest interface that I'm developing solely for my personal and daily usage.

Btw, because of this, the development of [ChatGP-Terminal](https://github.com/Lucho-A/ChatGP-Terminal) it is not more maintained by now.

### Version:

- 0.0.1-beta (experimental)

### Features/Assumptions/Scope/Whatever

- it supports window/memory context
- it supports 'pipe' redirection
- it supports history of queries during the session
- it supports changing system-roles, models, and adding pre-defined instructions to prompt
- at the moment, the output is only streamed (except when piped)
- allows to show the response's info
- allows to hide/show the model's 'thoughts' (like DeepSeek-R1 and latest models)
- supports only SSL/TLS (sorry, security by design, baby)

### Dependencies:

- libssl.so.3
- libcrypto.so.3
- libreadline.so.8
- libc.so.6

```
sudo apt-get install libssl-dev libcrypt-dev libreadline-dev
```
### Program compilation:

```
git clone https://github.com/lucho-a/Ollama-C-lient.git
```
```
cd Ollama-C-lient/src/
```
```
gcc -o ollama-c-lient Ollama-C-lient.c lib/* -lssl -lcrypto -lreadline
```

### Usage:

- $ ollama-c-lient [options]

The options supported are:

| Parameter | dataType:defaultValue _[boundaries]_ | Description
|:--------- | :----------------------------------- | -----------
|--version | N/A:N/A |
|--server-addr | string:127.0.0.1 | URL or IP of the server
|--server-port | int:443 _[1-65535_] | Listening port. Must be SSL/TLS.
|--model-file | string:NULL | File with the default model and parameters regarding to context, tokens, etc.
|--setting-file | string:NULL | File with the program settings: server info., timeouts, fonts, etc.
|--roles-file | string:NULL | File with the different roles for the model.
|--instructions-file | string:NULL | File with the different instructions for the model.
|--context-file | string:NULL | File where the interactions (except the queries ended with ';') will be stored.
|--show-response-info | N/A:false | Option for showing the responses' information, as tokens count, durations, etc.
|--show-thoughts | N/A:false | Option for showing what the model is 'thinking' in models like 'deepseek-r1'

Note: all options are optional (really?!).

On the other hand, some commands can be prompting:

| Command | Argument | Description |
|:------- |:---------|:----------- |
| flush;   | N/A     | the context in RAM will be cleared (this action won't delete any messages in the context file). I find it useful for avoiding any "misunderstanding" when I start or changing to a new topic.
| models;  | N/A     | shows available models.
| model;   | string* | change the model.
| roles;   | N/A     | show available roles;
| role;    | string* | change the system role (included into the file '--roles-file').
| instructions;   | N/A     | show available instructions;
| instruction;    | string* | add the instructions entered (included into the file '--instructions-file') to prompt history.

* If 'string' is empty, the model|role will change to the default one.

#### Considerations

- The sent messages are kept into the prompt history, so you can retrieve and editing them using the key up & down arrows.
- The sent messages & the responses are written into the context file if '--context-file' is specified.
- If **'--context-file'** is specified, the last '[MAX_MSG_CTX]' (parameter in modelfile) messages/responses are read when the program starts as context.
- So, if '[MAX_MSG_CTX]' > 0, and '--context-file' is not set up, the program will start without any context. Nevertheless, as long as chats succeed, they will be stored in RAM and taken into account in the successive interactions.
- If '[MAX_MSG_CTX]' = 0, the interactions won't be recorded into context file.
- The template of modelfile that must be used with '--model-file': [here](https://github.com/Lucho-A/Ollama-C-lient/tree/master/files-templates).
- **'--setting-file'** allows set different parameters. An example of file that should be used: [here](https://github.com/Lucho-A/Ollama-C-lient/tree/master/files-templates). The parameters set up in this file, override any parameter passed with any other option.
- the font format in 'settings' file must be ANSI.
- if not setting file is specified, uncolored is set by default.
- if **'--roles-file'** is specified, prompting 'role;' + the name of the role (specified into the file. V.gr.: role;newRole), will include the (new) role description as system role in the following chats. Example of file: [here](https://github.com/Lucho-A/Ollama-C-lient/tree/master/files-templates)
- if **'--instructions-file'** is specified, prompting 'instruction;' + the name of the instruction (specified into the file. V.gr.: instruction;newInstruction), will include the (new) instruction description into the prompt history so you can selected uping/dowing the arrow keys. Example of file: [here](https://github.com/Lucho-A/Ollama-C-lient/tree/master/files-templates)
- If the entered **prompt finish with ';'**, the query/response won't take into account the current context ([MAX_MSG_CTX]), won't be written to the context file, and won't be part of subsequent context messages.
- In case that you want to **input a new line** without submitting, just use 'alt+enter'. Same key combination for **exiting** when empty prompt.
- In case that pipes are used, like: 'df | ollama-c-lient >> file.txt', the output is always uncolored, no-streamed, and in RAW format.
- Crl-C cancel the responses.

### Examples:

#### Suggested:

```
$ ollama-c-lient --model-file ~/ollama/mymodelFile --setting-file ~/ollama/settingFile --roles-file ~/ollama/rolesFile --instructions-file ~/ollama/instructionsFile --context-file ~/ollama/contextFile
```

... with a 'model-file' like:

```
[MODEL]
modelNameAlreadyPulled
[KEEP_ALIVE]
30
[TEMP]
0.5
[MAX_MSG_CTX]
5
[MAX_TOKENS_CTX]
4096
[SYSTEM_ROLE]
You are the assistance and
bla,
bla, bla...
```
###### Note: since the incorporation of reasoning models, is not recommended incorporating a 'system-role'. Instead, just leave it blank and incorporate the instructions as part of the 'user-role' (see 'instructions;' and 'instruction;').

#### Others

```
$ ollama-c-lient --server-addr 192.168.2.10
$ ollama-c-lient --server-addr 192.168.2.10 --setting-file ~/ollama/settingFile --instructions-file ~/ollama/instructionsFile
$ ollama-c-lient --server-addr 192.168.1.50 --model-file ~/ollama/any1modelModelFile --context-file ~/ollama/context --show-response-info
$ ollama-c-lient --server-addr myownai.com --server-port 4433 --model-file ~/ollama/any2modelModelFile
$ ollama-c-lient --model-file ~/ollama/any3modelModelFile --show-thoughts
$ ollama-c-lient --roles-file ~/ollama/roles --context-file ~/ollama/context
$ (echo 'What can you tell me about my storage: ' && df) | ollama-c-lient --server-addr 192.168.5.123 --server-port 4433 --model-file ~/agents/modelFile --context-file ~/agents/dfAgentContextFile >> log-file.log
```

#### Bugs known/unknown

In line with a Top Down approach (and the beta & experimental stage), bugs will/can be found and the scoping and approach can change at any time.

