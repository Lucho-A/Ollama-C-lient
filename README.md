# Ollama-C-lient

C program for interacting with Ollama server from a Linux terminal. It is not meant to be a complete library at all. At the moment, it's just the simplest interface that I'm developing solely for my personal and daily usage.

Btw, because of this, the development of [ChatGP-Terminal](https://github.com/Lucho-A/ChatGP-Terminal) it is not more maintained by now.

### Version:

- 0.0.1 (beta/non-productive)

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
cd src/
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

#### Others

- The sent messages are kept into the prompt history, so you can retrieve and editing them using the key up & down arrows.
- In case that you want to input a new line without submitting, just use 'Alt+Enter'. Same key combination for exiting when empty prompt.
- The template of modelfile that must be used with '--modelfile': [here](https://github.com/Lucho-A/Ollama-C-lient/tree/master/modelfile).
- If the entered prompt finish with ';', the query doesn't take into account the context (and is not written into context file).
- If prompting 'flush;', the context in RAM (in line with modelfile->MAX_MSG_CTX parameter) will be removing (this action won't delete the messages in the context file). I find it useful for avoiding any "misunderstanding" when I start or change to a new topic.

### Examples:

```
$ ollama-c-lient --server-addr 192.168.1.50 --model-file ~/ollama/any1modelModelFile --context-file ~/ollama/context --show-response-info
$ ollama-c-lient --server-addr myownai.com --server-port 4433 --model-file ~/ollama/any2modelModelFile
$ ollama-c-lient --model-file ~/ollama/any3modelModelFile --uncolored
```

