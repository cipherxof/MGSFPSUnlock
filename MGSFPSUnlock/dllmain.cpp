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
std::filesystem::path sLogFile = "MGSFPSUnlock.log";
std::string sFixVer = "0.0.6";
std::filesystem::path sExePath;
std::filesystem::path sFixPath;

GameConfig Config;
HMODULE GameModule = GetModuleHandleA(NULL);
uintptr_t GameBase = (uintptr_t)GameModule;
mINI::INIStructure ConfigValues;

void ReadConfig()
{
    std::string configPath = (sExePath / sFixPath / "MGSFPSUnlock.ini").string();
    mINI::INIFile ini(configPath);
    if (!ini.read(ConfigValues)) 
    {
        ConfigValues["Settings"]["TargetFrameRate"] = "60";
        ini.generate(ConfigValues);
        spdlog::info("Config file not detected! Generating a new one at: {}", configPath);
    }
    else 
    {
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
    sExePath = std::filesystem::path(exePath).remove_filename();
    bool bFoundOnce = false;
    std::array<std::string, 4> paths = { "", "plugins", "scripts", "update" };
    for (const auto& path : paths)
    {
        auto filePath = sExePath / path / "MGSFPSUnlock.asi";
        if (std::filesystem::exists(filePath))
        {
            if (bFoundOnce) //multiple versions found
            {
                AllocConsole();
                FILE* dummy;
                freopen_s(&dummy, "CONOUT$", "w", stdout);
                std::string errorMessage = "DUPLICATE FILE ERROR: Duplicate MGSFPSUnlock.asi installations found! Please make sure to delete any old versions!\n";
                errorMessage.append("DUPLICATE FILE ERROR - Installation 1: ").append((sExePath / sFixPath / "MGSFPSUnlock.asi").string().append("\n"));
                errorMessage.append("DUPLICATE FILE ERROR - Installation 2: ").append(filePath.string());
                std::cout << errorMessage << std::endl;
                return FreeLibraryAndExitThread(GameModule, 1);
            }
            bFoundOnce = true;
            sFixPath = path;
        }
    }

    {
        try 
        {
            if (!std::filesystem::is_directory(sExePath / "logs"))
            {
                std::filesystem::create_directory(sExePath / "logs"); //create a "logs" subdirectory in the game folder to keep the main directory tidy.
            }
            logger = spdlog::basic_logger_mt("MGSFPSUnlock", (sExePath / "logs" / sLogFile).string(), true);
            logger->set_level(spdlog::level::debug);
            logger->flush_on(spdlog::level::debug);
            spdlog::set_default_logger(logger);
            spdlog::set_pattern(LOG_FORMAT_PREFIX ": %v");
            spdlog::info("MGSFPSUnlock v{} loaded.", sFixVer.c_str());
            spdlog::info("ASI plugin location: {}", (sExePath / sFixPath / "MGSFPSUnlock.asi").string());
            spdlog::info("Log file: {}", (sExePath / "logs" / sLogFile).string());
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

