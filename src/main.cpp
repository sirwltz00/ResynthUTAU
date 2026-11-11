#include "resynth/ParameterParser.hpp"
#include "resynth/UTAUInterface.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

static std::string utf8FromWide(const std::wstring& value) {
    if (value.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) throw std::runtime_error("Could not convert Windows command-line text to UTF-8");
    std::string result(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), needed, nullptr, nullptr);
    return result;
}

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc == 1) {
            std::cout << "ResynthUTAU Phase 1 foundation\n" << resynth::usageText();
            return 0;
        }
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc - 1));
        for (int i = 1; i < argc; ++i) args.push_back(utf8FromWide(argv[i]));
        const auto params = resynth::parseArguments(args);
        const auto report = resynth::renderUtauInvocation(params);
        std::cout << "Rendered " << report.output_frames << " frames at " << report.sample_rate << " Hz\n";
        for (const auto& warning : report.warnings) std::cerr << "[ResynthUTAU warning] " << warning << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ResynthUTAU: " << e.what() << '\n';
        return 2;
    }
}
#else
int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            std::cout << "ResynthUTAU Phase 1 foundation\n" << resynth::usageText();
            return 0;
        }
        std::vector<std::string> args(argv + 1, argv + argc);
        const auto params = resynth::parseArguments(args);
        const auto report = resynth::renderUtauInvocation(params);
        std::cout << "Rendered " << report.output_frames << " frames at " << report.sample_rate << " Hz\n";
        for (const auto& warning : report.warnings) std::cerr << "[ResynthUTAU warning] " << warning << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ResynthUTAU: " << e.what() << '\n';
        return 2;
    }
}
#endif
