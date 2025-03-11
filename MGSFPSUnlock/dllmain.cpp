#include "stdafx.h"
#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "ini.h"
#include "config.h"
#include "logger.h"
#include "MGS2/mgs2.h"
#include "MGS3/mgs3.h"

std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = "MGSFPSUnlock.log";
std::string sFixVer = "0.0.5";
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

void InitializeLogging()
{
    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(GameModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExePath = sExePath.remove_filename();

    // spdlog initialisation
    {
        try 
        {
            // Create 10MB truncated logger
            logger = std::make_shared<spdlog::logger>(sLogFile, std::make_shared<size_limited_sink<std::mutex>>(sExePath.string() + sLogFile, 10 * 1024 * 1024));
            spdlog::set_default_logger(logger);
            logger->set_level(spdlog::level::debug);
            spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l]: %v");
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
    InitializeLogging();
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
        MGS2_Initialize();
        break;
    case GameType::MGS3:
        spdlog::info("Found Game: MGS3 - Version: {}", versionstring);
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

