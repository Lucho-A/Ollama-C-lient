### ollama-c-lient-v0.0.3 Dev./Testing
#### date: 
#### severity: low
#### bugs-fixed:
- fixed mis-assigned realloc(), potential buffer overflow.
- fixed 'keep alive' value when requests
#### new-features:
- added parameter: --api-key.
- added parameter: --tools-file.
- added parameter: --execute-tools (***experimental***).
#### improvements:
- N/A
#### others:
- fixed OCL_VERSION define error.
- minor changes & code cleaned up.

### ollama-c-lient-v0.0.2
#### date: 2025/09/04
#### severity: medium
#### bugs-fixed:
- fixed --response-speed when --stdout-parsed wasn't set.
- fixed program blocking when --response-speed < 0.
- fixed showing thoughts when --stdout-chunked was set, and --show-thoughts was not.
#### new-features:
- added parameter: --no-think. Alignment with [Ollama v0.9.0 changes](https://github.com/ollama/ollama/releases/tag/v0.9.0)
- added parameters: --repeat-last-n, --repeat-penalty, --top-k, --top-p, --min-p. 
- added parameter: --stdout-json.
#### improvements:
- --stdout-chunked sets --stdout-parsed, as well.
#### others:
- potential memory leak fixed

### ollama-c-lient-v0.0.1
#### date: 2025/05/18
#### severity: N/A
#### bugs-fixed:
- N/A
#### new-features:
- N/A
#### improvements:
- N/A
#### others:
- N/A
