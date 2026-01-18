#include "/home/nikog/projects/2_buggy_AI/includes/argumentParser.h"
#include "/home/nikog/projects/2_buggy_AI/includes/OpenAiClient.h"
#include "/home/nikog/projects/2_buggy_AI/includes/ReportJson.h"
#include "/home/nikog/projects/2_buggy_AI/includes/GdbRunner.h"
#include "/home/nikog/projects/2_buggy_AI/includes/ValgrindRunner.h"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    try {
        ArgumentParser parser;
        parser.parse(argc, argv);

        if (parser.isVerbose()) {
            parser.printConfig();
        }

        const std::string program = parser.getTargetPath();
        const auto& passArgs = parser.getPassthroughArgs();

        // Runner optional
        RunResult gdbRes;
        ValgrindResult vgRes;
        RunResult* gdbPtr = nullptr;
        ValgrindResult* vgPtr = nullptr;

        if (parser.isGdbUsed()) {
            gdbRes = run_gdb(program, passArgs);
            gdbPtr = &gdbRes;
        }

        if (parser.isValgrindUsed()) {
            vgRes = run_valgrind(program, passArgs);
            vgPtr = &vgRes;
        }

        // Report JSON bauen
        std::string report = make_report_json(
            parser.getTargetPath(),
            parser.getFixDescription(),
            parser.isRecursive(),
            parser.isVerbose(),
            parser.getFileExtensions(),
            parser.getPassthroughArgs(),
            gdbPtr,
            vgPtr
        );

        // Optional: JSON in Datei schreiben
        if (!parser.getJsonOutFile().empty()) {
            std::ofstream out(parser.getJsonOutFile());
            out << report;
        }

        // OpenAI Debug
        OpenAIClient client;
        OpenAIResult r = client.debug_report(report);

        if (r.http_status >= 200 && r.http_status < 300) {
            std::cout << "\n===== DEBUG REPORT (OpenAI) =====\n";
            std::cout << (r.text.empty() ? r.raw_json : r.text) << "\n";
            std::cout << "=================================\n";
            return 0;
        }

        std::cerr << "OpenAI HTTP " << r.http_status << "\n" << r.raw_json << "\n";
        return 5;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}