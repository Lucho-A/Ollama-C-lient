# Ollama-C-lient
C program for interacting with Ollama server from a Linux terminal. It is not meant to be a complete library at all. It's just the simplest interface.

### Version: 0.0.1 (beta)

### Features/Assumptions/Whatever
<ul>
  <li>1</li>
  <li>2</li>
  <li>3</li>
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


### Example:

```
ollama-c-lient --server-addr 192.168.1.50 --model-file ~/ollama/phi3ModelFile --context-file ~/ollama/context --show-response-info
```

### Screenshots:
