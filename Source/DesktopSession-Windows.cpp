#include "DesktopSession.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace screencap
{
namespace
{
constexpr DWORD servicesSessionId = 0;
constexpr int exitFailed = 1;
constexpr auto taskName = "screencap-desktop-session";
constexpr auto pollInterval = std::chrono::milliseconds(200);
constexpr auto longestRun = std::chrono::minutes(30);

bool isInServicesSession()
{
    auto sessionId = DWORD {};

    if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId))
        return false;

    return sessionId == servicesSessionId;
}

std::string getExecutablePath()
{
    auto path = std::string(MAX_PATH, '\0');
    const auto length = GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return path;
}

// cmd.exe expands %name% even inside quotes; doubling the percent keeps it literal.
std::string quoteForBatchFile(const std::string& argument)
{
    auto quoted = std::string("\"");

    for (const auto character: argument)
    {
        if (character == '%')
            quoted += '%';

        quoted += character;
    }

    return quoted + "\"";
}

struct SessionFiles
{
    fs::path folder;
    fs::path batch = folder / "run.cmd";
    fs::path output = folder / "stdout.txt";
    fs::path errors = folder / "stderr.txt";
    fs::path exitCode = folder / "exit-code.txt";
    std::string command;
};

SessionFiles createSessionFiles(char** argv)
{
    auto files = SessionFiles {fs::temp_directory_path() / ("screencap-session-" + std::to_string(GetCurrentProcessId()))};
    fs::remove_all(files.folder);
    fs::create_directories(files.folder);
    files.command = quoteForBatchFile(getExecutablePath());

    for (auto argument = argv + 1; *argument != nullptr; ++argument)
        files.command += " " + quoteForBatchFile(*argument);

    return files;
}

void writeBatchFile(const SessionFiles& files)
{
    const auto partialExitCode = files.exitCode.string() + ".part";
    auto batch = std::ofstream(files.batch, std::ios::binary);
    batch << "@echo off\r\n"
          << "cd /d " << quoteForBatchFile(fs::current_path().string()) << "\r\n"
          << files.command << " > " << quoteForBatchFile(files.output.string()) << " 2> "
          << quoteForBatchFile(files.errors.string()) << "\r\n"
          << "echo %errorlevel% > " << quoteForBatchFile(partialExitCode) << "\r\n"
          << "move /y " << quoteForBatchFile(partialExitCode) << " " << quoteForBatchFile(files.exitCode.string())
          << " > nul\r\n";
}

bool runSchtasks(const std::string& arguments)
{
    auto commandLine = "schtasks.exe " + arguments;
    auto startup = STARTUPINFOA {sizeof(STARTUPINFOA)};
    startup.dwFlags = STARTF_USESTDHANDLES;
    auto process = PROCESS_INFORMATION {};

    if (!CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup, &process))
        return false;

    WaitForSingleObject(process.hProcess, INFINITE);
    auto exitCode = DWORD {};
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode == 0;
}

// A plain console program run by the task opens a terminal window on the user's desktop, which then covers
// what screencap captures; conhost --headless gives the batch file a console without any window.
bool createTask(const SessionFiles& files)
{
    const auto windowlessCommand = "conhost.exe --headless \\\"" + files.batch.string() + "\\\"";
    return runSchtasks(std::string("/create /f /tn ") + taskName + " /sc once /st 00:00 /it /tr \"" + windowlessCommand
                       + "\"");
}

bool startTask()
{
    return runSchtasks(std::string("/run /tn ") + taskName);
}

void deleteTask()
{
    runSchtasks(std::string("/delete /f /tn ") + taskName);
}

bool waitForFile(const fs::path& path)
{
    const auto deadline = std::chrono::steady_clock::now() + longestRun;

    while (!fs::exists(path))
    {
        if (std::chrono::steady_clock::now() > deadline)
            return false;

        std::this_thread::sleep_for(pollInterval);
    }

    return true;
}

std::string readFile(const fs::path& path)
{
    auto stream = std::ifstream(path, std::ios::binary);
    auto contents = std::ostringstream {};
    contents << stream.rdbuf();
    return contents.str();
}

int runAsScheduledTask(const SessionFiles& files)
{
    if (!createTask(files) || !startTask())
    {
        deleteTask();
        std::fputs("screencap: no desktop here (ssh runs in session 0) and the scheduled task that runs "
                   "screencap in the logged-in user's desktop could not start. Is that user logged in?\n",
                   stderr);
        return exitFailed;
    }

    const auto isFinished = waitForFile(files.exitCode);
    deleteTask();

    std::cout << readFile(files.output);
    std::cerr << readFile(files.errors);

    if (!isFinished)
    {
        std::fputs("screencap: the copy running in the user's desktop did not finish\n", stderr);
        return exitFailed;
    }

    return std::stoi(readFile(files.exitCode));
}
} // namespace

std::optional<int> runInUserDesktopSession(char** argv)
{
    if (!isInServicesSession())
        return std::nullopt;

    const auto files = createSessionFiles(argv);
    writeBatchFile(files);
    const auto exitCode = runAsScheduledTask(files);
    auto error = std::error_code {};
    fs::remove_all(files.folder, error);
    return exitCode;
}
} // namespace screencap
