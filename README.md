# Ollama-C-lient

C program for interacting with Ollama server from a Linux terminal. It is not meant to be a complete library at all. At the moment, it's just the simplest interface that I'm developing solely for my personal and daily usage.

Btw, because of this, the development of ChatGP-Terminal it is not more maintained by now.

### Version:
<ul>
  <li>0.0.1 (beta/non-productive)
</ul>

### Features/Assumptions/Scope/Whatever
<ul>
  <li>the program load the model when starts, and keeping it in server memory until exit</li>
  <li>it supports window context</li>
  <li>if the entered prompt finish with ';', the query doesn't take into account the context (and is not written into context file)</li>
  <li>at the moment, the output is only streamed</li>
  <li>allows to show the response's info'</li>
  <li>supports only SSL/TLS (sorry, security by design, baby)</li>
</ul>

### Dependencies:
<ul>
  <li>libssl.so.3</li>
  <li>libreadline.so.8</li>
  <li>libc.so.6</li>
</ul>

### Install:

```
git clone https://github.com/lucho-a/Ollama-C-lient.git
cd Ollama-C-lient/Src
gcc -o ollama-c-lient Ollama-C-lient.c lib/* -lssl -lreadline
```

### Usage:

//TODO

### Example:

```
ollama-c-lient --server-addr 192.168.1.50 --server-port 443 --model-file ~/ollama/mistralModelFile --context-file ~/ollama/context --show-response-info
```

