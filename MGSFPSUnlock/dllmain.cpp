#include "stdafx.h"
#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "ini.h"
#include "config.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "MGS2/mgs2.h"
#include "MGS3/mgs3.h"

#define LOG_FORMAT_PREFIX "[%Y-%m-%d %H:%M:%S.%e] [MGSFPSUnlock] [%l]"

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

void InitializeLogger()
{
    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(GameModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExePath = sExePath.remove_filename();

    {
        try 
        {
            logger = spdlog::basic_logger_mt("MGSFPSUnlock", "MGSFPSUnlock.log", true);
            logger->set_level(spdlog::level::debug);
            logger->flush_on(spdlog::level::debug);
            spdlog::set_default_logger(logger);
            spdlog::set_pattern(LOG_FORMAT_PREFIX ": %v");
            spdlog::info("MGSFPSUnlock v{} loaded.", sFixVer.c_str());
            spdlog::info("Log file: {}", sExePath.string() + sLogFile);
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
    InitializeLogger();
    ReadConfig();

    spdlog::info("Config Loaded");
    spdlog::info("Target Framerate: {}", ConfigValues["Settings"]["TargetFrameRate"]);

    GetGameType(GameModule, Config.gameType);
    Config.gameVersion = GetGameVersion(GameModule);
    Config.targetFramerate = std::stoi(ConfigValues["Settings"]["TargetFrameRate"]);

    if (Config.gameVersion == 0) 
    {
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
        spdlog::set_pattern(LOG_FORMAT_PREFIX " MGS2: %v");
        MGS2_Initialize();
        break;
    case GameType::MGS3:
        spdlog::info("Found Game: MGS3 - Version: {}", versionstring);
        spdlog::set_pattern(LOG_FORMAT_PREFIX " MGS3: %v");
        MGS3_Initialize();
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

