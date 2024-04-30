# Ollama-C-lient

C program for interacting with Ollama server from a Linux terminal. It is not meant to be a complete library at all. At the moment, it's just the simplest interface that I'm developing solely for my personal and daily usage.

Btw, because of this, the development of [ChatGP-Terminal](https://github.com/Lucho-A/ChatGP-Terminal) it is not more maintained by now.

### Version:

- 0.0.1 (beta)

### Features/Assumptions/Scope/Whatever

- the program load the model when starts, and keeping it in server memory until exit
- it supports window context
- at the moment, the output is only streamed
- allows to show the response's info
- supports only SSL/TLS (sorry, security by design, baby)

### Dependencies:

- libssl.so.3
- libreadline.so.8
- libc.so.6

### Install:

```
git clone https://github.com/lucho-a/Ollama-C-lient.git
cd Ollama-C-lient/src/
gcc -o ollama-c-lient Ollama-C-lient.c lib/* -lssl -lreadline
```

### Usage:

- $ ollama-c-lient [options]

The options (and arguments -> 'dataType:defaultValue [boundaries]') supported are:

- --version             N/A
- --server-addr         string:127.0.0.1
- --server-port         int:443 [1-65535]
- --model-file          string*
- --context-file        string
- --show-response-info  N/A:FALSE
- --uncolored           N/A:FALSE

Note: * mandatory options.

#### Considerations

- The sent messages are kept into the prompt history, so you can retrieve and editing them using the key up & down arrows.
- The sent messages & the responses are written into the context file if '--context-file' is specified.
- If '--context-file' is specified, the last '[MAX_MSG_CTX]' (parameter in modelfile) messages/responses are read when the program starts as context.
- So, if '[MAX_MSG_CTX]' > 0, and '--context-file' is not set up, the program will start without any context. Nevertheless, as long as chats succeed, they will be stored in RAM and taken into account in the successive interactions.
- If prompting 'flush;', the context in RAM will be cleared (this action won't delete any messages in the context file). I find it useful for avoiding any "misunderstanding" when I start or changing to a new topic.
- The template of modelfile that must be used with '--modelfile': [here](https://github.com/Lucho-A/Ollama-C-lient/tree/master/modelfile).
- If the entered prompt finish with ';', the query/response won't take into account the current context ([MAX_MSG_CTX]), won't be written to the context file, and won't be part of subsequent context messages.
- In case that you want to input a new line without submitting, just use 'alt+enter'. Same key combination for exiting when empty prompt.

### Examples:

#### Suggested set up:

```
$ ollama-c-lient --server-addr myownai.com --server-port 4433 --model-file ~/ollama/mymodelModelFile --context-file ~/ollama/context
```

... with a modelFile like:

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
$ ollama-c-lient --server-addr 192.168.1.50 --model-file ~/ollama/any1modelModelFile --context-file ~/ollama/context --show-response-info
$ ollama-c-lient --server-addr myownai.com --server-port 4433 --model-file ~/ollama/any2modelModelFile
$ ollama-c-lient --model-file ~/ollama/any3modelModelFile --uncolored
```
