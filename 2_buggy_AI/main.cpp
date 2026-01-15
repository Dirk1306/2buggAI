#include "ClaudeClient.h"
#include "ReportJson.h"
#include "GdbRunner.h"
#include "ValgrindRunner.h"

#include <cstdlib>
#include <iostream>
#include <string>

static std::string getenv_str(const char* k) {
    const char* v = std::getenv(k);
    return v ? std::string(v) : std::string();
}

int main(int argc, char** argv) {
    // ... dein ArgumentParser + Runner-Calls ...
    // Beispiel: Report bauen (du ersetzt das mit deinem echten Flow)
    RunResult gdbRes = run_gdb("./dein_programm", {});
    RunResult* gdbPtr = &gdbRes;

    ValgrindResult vgRes = run_valgrind("./dein_programm", {});
    ValgrindResult* vgPtr = &vgRes;

    std::string report = make_report_json(
        "./dein_programm",
        "Bitte debuggen",
        false,
        true,
        {".cpp", ".h"},
        {},
        gdbPtr,
        vgPtr
    );

    // --- Claude config automatisch aus ENV ---
    std::string apiKey = getenv_str("ANTHROPIC_API_KEY");
    std::string model  = getenv_str("ANTHROPIC_MODEL");
    std::string base   = getenv_str("ANTHROPIC_BASE_URL");

    if (apiKey.empty()) {
        std::cerr << "ANTHROPIC_API_KEY ist nicht gesetzt.\n";
        return 2;
    }
    if (model.empty()) model = "claude-sonnet-4-5";
    if (base.empty())  base  = "https://api.anthropic.com";

    // --- Preflight, bevor du den großen Report schickst ---
    std::string preflight_err;
    if (!claude_preflight_models(apiKey, base, &preflight_err)) {
        std::cerr << "Claude Preflight fehlgeschlagen: " << preflight_err << "\n";
        return 3;
    }

    // --- Prompt bauen: "wo liegt der Fehler?" ---
    std::string system_prompt =
        "Du bist ein Senior C++ Debugger. Antworte kurz und konkret.";

    std::string user_text =
        "Analysiere diesen Debug-Report (JSON) und gib:\n"
        "1) Root cause (1 Satz)\n"
        "2) Wo im Code wahrscheinlich (Datei/Funktion/Zeile falls ableitbar)\n"
        "3) Beweisstellen aus dem Output (zitiere relevante Zeilen)\n"
        "4) Konkrete Fix-Vorschläge (bullet points)\n\n"
        "REPORT_JSON:\n" + report;

    ClaudeResponse cr = call_claude_messages(apiKey, model, 1200, system_prompt, user_text, base);

    if (!cr.error.empty()) {
        std::cerr << "Claude error: " << cr.error << "\n";
        std::cerr << "HTTP " << cr.http_status << "\n" << cr.raw_body << "\n";
        return 4;
    }
    if (cr.http_status != 200) {
        std::cerr << "Claude HTTP " << cr.http_status << "\n" << cr.raw_body << "\n";
        return 5;
    }

    std::cout << "\n===== CLAUDE DEBUG REPORT =====\n";
    std::cout << cr.assistant_text << "\n";
    std::cout << "===============================\n";
    return 0;
}
