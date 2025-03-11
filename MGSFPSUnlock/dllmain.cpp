#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "ini.h"
#include "config.h"
#include "MGS2/mgs2.h"
#include "MGS3/mgs3.h"

GameConfig Config;
HMODULE GameModule = GetModuleHandleA(NULL);
uintptr_t GameBase = (uintptr_t)GameModule;
mINI::INIStructure ConfigValues;

void ReadConfig()
{
    mINI::INIFile ini("MGSFPSUnlock.ini");
    ini.read(ConfigValues);
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
    ReadConfig();
    GetGameType(GameModule, Config.gameType);
    Config.gameVersion = GetGameVersion(GameModule);
    Config.targetFramerate = std::stoi(ConfigValues["Settings"]["TargetFrameRate"]);

    if (Config.gameVersion == 0)
        return false;

    switch (Config.gameType)
    {
    case GameType::MGS2:
        MGS2_Init();
        break;
    case GameType::MGS3:
        MGS3_Init();
        break;
    default:
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

