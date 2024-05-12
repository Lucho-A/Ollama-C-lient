# Ollama-C-lient

C program for interacting with Ollama server from a Linux terminal. It is not meant to be a complete library at all. At the moment, it's just the simplest interface that I'm developing solely for my personal and daily usage.

Btw, because of this, the development of [ChatGP-Terminal](https://github.com/Lucho-A/ChatGP-Terminal) it is not more maintained by now.

### Version:

- 0.0.1 (beta)

### Features/Assumptions/Scope/Whatever

- if specify, the program load the model when starts, and keeping it in server memory until exit or model changing
- it supports window context
- at the moment, the output is only streamed
- allows to show the response's info
- supports only SSL/TLS (sorry, security by design, baby)

### Dependencies:

- libssl.so.3
- libcrypto.so.3
- libreadline.so.8
- libc.so.6

### Install:

```
git clone https://github.com/lucho-a/Ollama-C-lient.git
cd Ollama-C-lient/src/
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
|--roles-file | string:NULL | File with the different roles/instructions for the model.
|--context-file | string:NULL | File where the interactions (except the queries ended with ';') will be stored.
|--show-response-info | N/A:FALSE | Option for showing the responses' information, as tokens count, durations, etc.

Note: all options are optional (really?!).

On the other hand, some commands can be prompting:

| Command | Argument | Description |
|:------- |:---------|:----------- |
| flush;   | N/A      | the context in RAM will be cleared (this action won't delete any messages in the context file). I find it useful for avoiding any "misunderstanding" when I start or changing to a new topic.
| model;   | string*   | change the model.
| role;    | string*   | role name included into the file ('--roles-file').

* If 'string' is empty, the model|role will change to the default one.

#### Considerations

- The sent messages are kept into the prompt history, so you can retrieve and editing them using the key up & down arrows.
- The sent messages & the responses are written into the context file if '--context-file' is specified.
- If **'--context-file'** is specified, the last '[MAX_MSG_CTX]' (parameter in modelfile) messages/responses are read when the program starts as context.
- So, if '[MAX_MSG_CTX]' > 0, and '--context-file' is not set up, the program will start without any context. Nevertheless, as long as chats succeed, they will be stored in RAM and taken into account in the successive interactions.
- The template of modelfile that must be used with '--model-file': [here](https://github.com/Lucho-A/Ollama-C-lient/tree/master/model-file).
- **'--setting-file'** allows set different parameters. An example of file that should be used: [here](https://github.com/Lucho-A/Ollama-C-lient/tree/master/setting-file). The parameters set up in this file, override any parameter passed with any other option.
- the font format in 'settings' file must be ANSI.
- if not setting file is specified, uncolored is set by default.
- if **'--roles-file'** is specified, with 'role;' + the name of the role into the specified file, role description is loaded and included as futures system role. Example of file: [here](https://github.com/Lucho-A/Ollama-C-lient/tree/master/roles-file)
- If the entered **prompt finish with ';'**, the query/response won't take into account the current context ([MAX_MSG_CTX]), won't be written to the context file, and won't be part of subsequent context messages.
- In case that you want to **input a new line** without submitting, just use 'alt+enter'. Same key combination for **exiting** when empty prompt.

### Examples:

#### Suggested:

```
$ ollama-c-lient --model-file ~/ollama/mymodelFile --setting-file ~/ollama/settingFile --roles-file ~/ollama/rolesFile --context-file ~/ollama/contextFile
```

... with a 'model-file' like:

```
[MODEL]
modelNameAlreadyPulled
[TEMP]
0.5
[MAX_MSG_CTX]
5
[MAX_TOKENS_CTX]
4096
[MAX_TOKENS]
4096
[SYSTEM_ROLE]
You are the assistance and
bla,
bla, bla...
```

#### Others

```
$ ollama-c-lient --server-addr 192.168.2.10
$ ollama-c-lient --server-addr 192.168.2.10 --setting-file ~/ollama/settingFile
$ ollama-c-lient --server-addr 192.168.1.50 --model-file ~/ollama/any1modelModelFile --context-file ~/ollama/context --show-response-info
$ ollama-c-lient --server-addr myownai.com --server-port 4433 --model-file ~/ollama/any2modelModelFile
$ ollama-c-lient --model-file ~/ollama/any3modelModelFile
$ Ollama-C-lient --roles-file ~/ollama/roles --context-file ~/ollama/context
```

#### Bugs known/unknown

In line with a Top Down approach (and the Beta stage), several bugs can be found.





