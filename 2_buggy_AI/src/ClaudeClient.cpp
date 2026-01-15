#include "ClaudeClient.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <sstream>
#include <vector>

using nlohmann::json;

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(ptr, size * nmemb);
    return size * nmemb;
}

static std::string join_url(const std::string& base, const std::string& path) {
    if (!base.empty() && base.back() == '/') return base.substr(0, base.size() - 1) + path;
    return base + path;
}

static ClaudeResponse http_request(
    const std::string& method,
    const std::string& url,
    const std::vector<std::string>& headers_vec,
    const std::string* body_json,   // nullptr = kein body
    long timeout_seconds = 120
) {
    ClaudeResponse res;

    CURL* curl = curl_easy_init();
    if (!curl) {
        res.error = "curl_easy_init failed";
        return res;
    }

    struct curl_slist* headers = nullptr;
    for (const auto& h : headers_vec) headers = curl_slist_append(headers, h.c_str());

    std::string response_body;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);

    if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (body_json) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_json->c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body_json->size());
        }
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        if (body_json) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_json->c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body_json->size());
        }
    }

    CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res.http_status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    res.raw_body = std::move(response_body);

    if (code != CURLE_OK) {
        res.error = std::string("curl_easy_perform failed: ") + curl_easy_strerror(code);
        return res;
    }

    return res;
}

static std::string extract_assistant_text(const std::string& body, std::string& err) {
    try {
        json j = json::parse(body);

        // Error-Shape (falls vorhanden)
        if (j.contains("error")) {
            err = j.dump(2);
            return "";
        }

        // Success: content = [{"type":"text","text":"..."}]
        if (!j.contains("content") || !j["content"].is_array()) return "";

        std::ostringstream out;
        for (const auto& block : j["content"]) {
            if (block.contains("type") && block["type"].is_string()
                && block["type"].get<std::string>() == "text"
                && block.contains("text") && block["text"].is_string()) {
                out << block["text"].get<std::string>();
            }
        }
        return out.str();
    } catch (const std::exception& e) {
        err = e.what();
        return "";
    }
}

bool claude_preflight_models(const std::string& api_key,
                             const std::string& base_url,
                             std::string* error_out) {
    // GET /v1/models (klein, prüft Verbindung + Key)
    const std::string url = join_url(base_url, "/v1/models");

    std::vector<std::string> headers = {
        "anthropic-version: 2023-06-01",
        "x-api-key: " + api_key
    };

    ClaudeResponse r = http_request("GET", url, headers, nullptr, 30);

    if (!r.error.empty()) {
        if (error_out) *error_out = r.error;
        return false;
    }

    // 200 = ok; alles andere (401/403) = verbunden, aber Auth/Key Problem
    if (r.http_status != 200) {
        if (error_out) {
            std::ostringstream os;
            os << "Preflight HTTP " << r.http_status << ": " << r.raw_body;
            *error_out = os.str();
        }
        return false;
    }

    return true;
}

ClaudeResponse call_claude_messages(
    const std::string& api_key,
    const std::string& model,
    int max_tokens,
    const std::string& system_prompt,
    const std::string& user_text,
    const std::string& base_url
) {
    const std::string url = join_url(base_url, "/v1/messages");

    json payload;
    payload["model"] = model;
    payload["max_tokens"] = max_tokens;
    // system ist top-level (keine "system" role in messages)
    if (!system_prompt.empty()) payload["system"] = system_prompt;
    payload["messages"] = json::array({
        { {"role", "user"}, {"content", user_text} }
    });

    std::string payload_str = payload.dump();

    std::vector<std::string> headers = {
        "anthropic-version: 2023-06-01",
        "x-api-key: " + api_key,
        "content-type: application/json"
    };

    ClaudeResponse res = http_request("POST", url, headers, &payload_str, 120);

    if (res.error.empty()) {
        res.assistant_text = extract_assistant_text(res.raw_body, res.error);
    }
    return res;
}
