#include "stdafx.h"
#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "ini.h"
#include "config.h"
#include "MGS2/mgs2.h"
#include "MGS3/mgs3.h"


#ifndef SPDLOG
#define FMT_UNICODE 0
#include "spdlog/spdlog.h"
#include "spdlog/sinks/base_sink.h"
#define SPDLOG
#endif



// Logger
std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = "MGSFPSUnlock.log";
std::string sFixVer = "0.0.6";
std::filesystem::path sExePath;

GameConfig Config;
HMODULE GameModule = GetModuleHandleA(NULL);
uintptr_t GameBase = (uintptr_t)GameModule;
mINI::INIStructure ConfigValues;

void ReadConfig()
{
    mINI::INIFile ini("MGSFPSUnlock.ini");
    ini.read(ConfigValues);
}

// Spdlog sink (truncate on startup, single file)
template<typename Mutex>
class size_limited_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit size_limited_sink(const std::string& filename, size_t max_size)
        : _filename(filename), _max_size(max_size) {
        truncate_log_file();

        _file.open(_filename, std::ios::app);
        if (!_file.is_open()) {
            throw spdlog::spdlog_ex("Failed to open log file " + filename);
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (std::filesystem::exists(_filename) && std::filesystem::file_size(_filename) >= _max_size) {
            return;
        }

        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);

        _file.write(formatted.data(), formatted.size());
        _file.flush();
    }

    void flush_() override {
        _file.flush();
    }

private:
    std::ofstream _file;
    std::string _filename;
    size_t _max_size;

    void truncate_log_file() {
        if (std::filesystem::exists(_filename)) {
            std::ofstream ofs(_filename, std::ofstream::out | std::ofstream::trunc);
            ofs.close();
        }
    }
};

void Logging()
{
    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(GameModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExePath = sExePath.remove_filename();

    // spdlog initialisation
    {
        try {
            // Create 10MB truncated logger
            logger = std::make_shared<spdlog::logger>(sLogFile, std::make_shared<size_limited_sink<std::mutex>>(sExePath.string() + sLogFile, 10 * 1024 * 1024));
            spdlog::set_default_logger(logger);

            spdlog::flush_on(spdlog::level::debug);
            spdlog::info("----------");
            spdlog::info("MGSFPSUnlock v{} loaded.", sFixVer.c_str());
            spdlog::info("----------");
            spdlog::info("Log file: {}", sExePath.string() + sLogFile);
            spdlog::info("----------");
        }
        catch (const spdlog::spdlog_ex& ex) {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialisation failed: " << ex.what() << std::endl;
            FreeLibraryAndExitThread(GameModule, 1);
        }
    }
}


DWORD WINAPI MainThread(LPVOID lpParam)
{
    Logging();
    ReadConfig();
    spdlog::info("Config Loaded");
    spdlog::info("Target Framerate: {}", ConfigValues["Settings"]["TargetFrameRate"]);
    spdlog::info("----------");
    GetGameType(GameModule, Config.gameType);
    Config.gameVersion = GetGameVersion(GameModule);
    Config.targetFramerate = std::stoi(ConfigValues["Settings"]["TargetFrameRate"]);

    if (Config.gameVersion == 0) {
        spdlog::error("Unable to get game version, closing!");
        return false;
    }

    //convert gameVersion to a string
    std::stringstream stream;
    stream << std::hex << Config.gameVersion;
    std::string versionstring(stream.str());

    switch (Config.gameType)
    {
    case GameType::MGS2:
        spdlog::info("Found Game: MGS2 - Version: {}", versionstring);
        spdlog::info("----------");
        MGS2_Init();
        break;
    case GameType::MGS3:
        spdlog::info("Found Game: MGS3 - Version: {}", versionstring);
        spdlog::info("----------");
        MGS3_Init();
        break;
    default:
        spdlog::error("Unknown game!");
        break;
    }

    return true;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, NULL, MainThread, NULL, NULL, NULL);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

