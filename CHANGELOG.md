### ollama-c-lient-v0.0.3 Dev./Testing
#### date: 
#### severity: low
#### bugs-fixed:
- fixed mis-assigned realloc(), potential buffer overflow.
- fixed 'keep alive' value when requests
- fixed un-inclusion of message as context when first interaction was created. 
#### new-features:
- added parameter: '--api-key'.
- added parameter: '--tools-file'.
- added parameter: '--execute-tools' (***experimental***).
- added parameter: '--stdout-buffer-size'. Set the minimum char length of the stream before to stdout.
- added parameter: '--exclude-chars' (***experimental***). It sets the chars to be excluded from response (vgr. --exclude-chars '*-_'). At the moment chars with escape sequence are not supported. 
#### improvements:
- color format validation
- '--max-msgs-ctx' == 0, save the interaction into context file.
#### others:
- fixed OCL_VERSION define error.
- added compatibility with Ollama Cloud ([Ollama v0.12.0 changes](https://github.com/ollama/ollama/releases/tag/v0.12.0)) (1) (2)
- '--show-loading-models' de-scoped (for this update, and for legacy, no errors arises in case of its inclusion).
- potential memory leak fixed
- minor changes & code cleaned up.

(1) for some reason, models other than gpt don't support 'system' role, returning: "upstream error". Assumed by now that this is not intentional and will be fixed in future Ollama updates. 

(2) for some reason, the responses don't included: 'load_duration', 'prompt_eval_duration', nor 'eval_duration'. Therefore, 'Tokens per sec.' = inf (set to 0).

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
