#pragma once
#include <string>

struct ClaudeResponse {
    long http_status = 0;
    std::string raw_body;        // komplette JSON Antwort
    std::string assistant_text;  // zusammengefügter Text aus content[]
    std::string error;           // falls etwas schiefgeht
};

// Preflight: prüft Verbindung + Auth über GET /v1/models
bool claude_preflight_models(const std::string& api_key,
                             const std::string& base_url,
                             std::string* error_out = nullptr);

// Messages API call: POST /v1/messages
ClaudeResponse call_claude_messages(
    const std::string& api_key,
    const std::string& model,
    int max_tokens,
    const std::string& system_prompt,
    const std::string& user_text,
    const std::string& base_url = "https://api.anthropic.com"
);
