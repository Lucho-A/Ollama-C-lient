### ollama-c-lient-v0.0.2 - Under Dev./Testing

#### date: 
#### severity: medium
#### bugs-fixed:
- fixed --response-speed when --stdout-parsed wasn't set.
- fixed program blocking when --response-speed < 0.
#### new-features:
- added parameter: --no-think. Alignment with [Ollama v0.9.0 changes](https://github.com/ollama/ollama/releases/tag/v0.9.0)
- added parameters: --repeat-last-n, --repeat-penalty, --top-k, --top-p, --min-p. 
- added parameter: --stdout-json.
#### improvements:
- --stdout-chunked set --stdout-parsed, as well.
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
