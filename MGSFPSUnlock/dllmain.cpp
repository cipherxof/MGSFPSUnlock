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
std::string sFixPath;

GameConfig Config;
HMODULE GameModule = GetModuleHandleA(NULL);
uintptr_t GameBase = (uintptr_t)GameModule;
mINI::INIStructure ConfigValues;

void ReadConfig()
{
    std::string configPath = sExePath.string() + sFixPath + "MGSFPSUnlock.ini";
    mINI::INIFile ini(configPath);
    if (!ini.read(ConfigValues)) {
        ConfigValues["Settings"]["TargetFrameRate"] = "60";
        ini.generate(ConfigValues);
        spdlog::info("Config file not detected! Generating a new one at: {}", configPath);
    }
    else {
        spdlog::info("Config file loaded: {}", configPath);
    }
    Config.targetFramerate = std::stoi(ConfigValues["Settings"]["TargetFrameRate"]);
    spdlog::info("Config Parse: Target Framerate: {}", Config.targetFramerate);
}

void InitializeLogger()
{
    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(GameModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExePath = sExePath.remove_filename();

    std::string paths[4] = { "", "plugins\\", "scripts\\", "update\\" };
    for (int i = 0; i < (sizeof(paths) / sizeof(paths[0])); i++) {
        if (std::filesystem::exists(sExePath.string() + paths[i] + "MGSFPSUnlock.asi")) {
            sFixPath = paths[i];
            break;
        }
    }

    {
        try 
        {
            if (!std::filesystem::is_directory(sExePath.string() + "logs"))
                std::filesystem::create_directory(sExePath.string() + "logs"); //create a "logs" subdirectory in the game folder to keep the main directory tidy.
            logger = spdlog::basic_logger_mt("MGSFPSUnlock", sExePath.string() + "logs\\" + sLogFile, true);
            logger->set_level(spdlog::level::debug);
            logger->flush_on(spdlog::level::debug);
            spdlog::set_default_logger(logger);
            spdlog::set_pattern(LOG_FORMAT_PREFIX ": %v");
            spdlog::info("MGSFPSUnlock v{} loaded.", sFixVer.c_str());
            spdlog::info("ASI plugin location: {}", sExePath.string() + sFixPath + "MGSFPSUnlock.asi");
            spdlog::info("Log file: {}", sExePath.string() + "logs\\" + sLogFile);
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

    GetGameType(GameModule, Config.gameType);
    Config.gameVersion = GetGameVersion(GameModule);
    if (Config.gameVersion == 0)
    {
        spdlog::error("Unable to get game version, closing!");
        return false;
    }

    ReadConfig(); //Read the config after checking gameVersion, otherwise any regeneration logs will be cleared when the launcher closes.


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

