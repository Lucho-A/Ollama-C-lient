# Ollama-C-lient

C program for interacting with [Ollama](https://github.com/ollama/ollama) server from a Linux terminal. It is not meant to be a complete library at all. At the moment, it's just the simplest interface that I'm developing solely for my personal and daily usage.

### Version:

- 0.0.1

### Disclaimer

- supports only SSL/TLS (sorry, security by design, baby)

###### Note: self signed certs are allowed.

### Dependencies:

- libssl.so.3
- libcrypto.so.3
- libc.so.6

```
sudo apt-get install libssl-dev libcrypt-dev
```
### Program compilation:

```
git clone https://github.com/lucho-a/Ollama-C-lient.git
```
```
cd Ollama-C-lient/src/
```
```
gcc -o ollama-c-lient Ollama-C-lient.c lib/* -lssl -lcrypto
```

### Usage:

- $ ollama-c-lient [options]

The options supported are:

| <img width=200/> Parameter | data-type:default-value _[boundaries]_ | Description |
|:- | :- | -- |
|--version | N/A:N/A | |
|--help | N/A:N/A | |
|--server-addr | string:"127.0.0.1" | URL or IP of the server |
|--server-port | int:443 _[1-65535]_ | listening port. Must be SSL/TLS. |
|--response-speed | int:0 _[>=0]_ | in microseconds, if > 0, the responses will be sending out to stdout at the interval set up. |
|--socket-conn-to | int:5 _[>=0]_ | in seconds, set up the connection time out. |
|--socket-send-to | int:5 _[>=0]_ | in seconds, set up the sending time out. |
|--socket-recv-to | int:15 _[>=0]_ | in seconds, set up the receiving time out. When a model is loading, this value is set to 120s to prevent false errors reporting. |
|--model | string:NULL | model to use. |
|--temperature | double:0.5 _[>=0]_ | set the temperature of the model. |
|--seed | int:0 _[>=0]_ | set the seed of the model. |
|--keep-alive | int:300 _[>=0]_ | in seconds, tell to the server how many seconds the model will be available until unloaded. |
|--max-msgs-ctx | int:3 _[>=0]_ | set the maximum messages to be added as context in the messages. |
|--max-msgs-tokens | int:4096 _[>=0]_ | set the maximum tokens. |
|--system-role | string:"" | set the system role. Override '--system-role-file'. |
|--system-role-file | string:NULL | set the path to the file that include the system role. |
|--context-file | string:NULL | file where the interactions (except the queries ended with ';') will be stored. |
|--static-context-file | string:NULL | file where the interactions included into it (separated by '\t') will be include (statically) as interactions in every query sent to the server. This interactions cannot be flushed, and they don't count as '--max-msgs-ctx' (it does as '--max-msgs-tokens'). |
|--image-file | string:NULL | Image file to attach to the query. |
|--color-font-response | string:"00;00;00" | in ANSI format, set the color used for responses. |
|--color-font-system | string:"00;00;00" | in ANSI format, set the color used for program's messages. |
|--color-font-info | string:"00;00;00" | in ANSI format, set the color used for response's info ('--show-response-info'). |
|--color-font-error | string:"00;00;00" | in ANSI format, set the color used for errors. |
|--show-response-info | N/A:false | showing the responses' information, as tokens count, duration, etc. |
|--show-thoughts | N/A:false | showing what the model is 'thinking' in reasoning models like 'deepseek-r1'. |
|--show-models | N/A:false | show the models available. |
|--show-loading-models | N/A:false | show a message when a model is loading. |
|--stdout-parsed | N/A:false | parsing the output (useful for speeching/chatting). |
|--stdout-chunked | N/A:false | chunking the output by paragraph (particularly useful for speeching). Only works if '--stdout-parsed' was set. |

###### Note: all options are optional (really?!).

### Considerations

- The sent messages & the responses are written into the context file if '--context-file' is specified.
- If **'--context-file'** is specified and it doesn't exit, it will be created if possible.
- If **'--context-file'** is specified, the latest X messages/responses (parameter '--max-msgs-ctx') are read when the program starts as context.
- So, if '--max-msgs-ctx' > 0, and '--context-file' is not set up, the program will start without any context. Nevertheless, as long as chats succeed, they will be stored in RAM and taken into account in the successive interactions. (1)
- If '--max-msgs-ctx' == 0, the interactions won't be recorded into context file.
- If '--stdout-chunked', '--response-speed' is deprecated.
- the font format in '--font-...' must be ANSI ("XX;XX;XX").
- If the entered **prompt finish with ';'**, the query/response won't take into account the current context ('--max-msgs-ctx') and won't be written to the context file,
- If the entered **prompt finish with ';'**, the query/response won't be part of subsequent context messages. (1)
- Crl-C cancel the responses.

###### (1) only relevant for developing purposes using the library.

### Examples:

#### Chatting

Just using a script like:

```
#!/bin/bash
OCL="/home/user/ollama-c-lient/ollama-c-lient
    --server-addr 192.168.1.234
    --server-port 443
    --model mistral
    --temperature 0.6
    --response-speed 15000
    --max-msgs-ctx 5
    --max-msgs-tokens 8196
    --keep-alive 1800
    --context-file /home/user/ollama-c-lient/chat.context
    --system-role-file /home/user/ollama-c-lient/chat.role
    --color-font-response '0;0;90'
    --color-font-system '0;0;37'
    --color-font-info '0;2;33'
    --color-font-error '0;0;31'
    --stdout-parsed"
echo
while true; do
    read -e -p "-> " input
    if [[ $input == "" ]]; then
        break
    fi
    echo "$input" | $OCL
    history -s $input
done
history -cw
echo
exit 0
```

#### Scripting/Agents
```
(echo 'What can you tell me about my storage: ' && df) | ./ollama-c-lient --server-addr 192.168.5.123 --server-port 4433 --model deepseek-r1 --context-file ~/agents/dfAgentContextFile.context --stdout-parsed >> log-file.log
```
```
./ollama-c-lient --model deepseek-r1 < prompt.txt
```
```
(echo 'What can you tell me about the content of this file?: ' && cat /home/user/file.txt) | ./ollama-c-lient --server-addr 192.168.5.123 --server-port 4433 --model deepseek-r1 --stdout-parsed < prompt.txt
```
```
(echo -n 'At ' && date +"%Y-%m-%d %H:%M:%S" && echo 'What can you tell me about current processes?: ' && ps ax) | ./ollama-c-lient --server-addr 192.168.1.2 --server-port 443 --model mistral --stdout-parsed --stdout-chunked | grep 'Ollama-C-lient'
```
```
(echo 'Tell me a prompt about whatever: ' && cat whatever.txt) | ./ollama-c-lient --server-addr 192.168.1.2 --server-port 443 --model mistral | ./ollama-c-lient --server-addr 192.168.1.2 --server-port 443 --model deepseek-r1:14b --stdout-parsed
```
```
(echo 'What can you tell me about about this paint? ') | ./ollama-c-lient --model gemma3:12b --stdout-parsed --response-speed 15000 --color-font-response "0;0;90" --image-file ~/paints/van-gogh.jpg
```
###### Note: since the incorporation of reasoning models, is not recommended incorporating a 'system-role'. Instead, just leave it blank and incorporate the instructions as part of the 'user-role'.
