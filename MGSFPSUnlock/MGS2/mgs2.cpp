#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "ini.h"
#include "config.h"
#ifndef SPDLOG
#define FMT_UNICODE 0
#include "spdlog/spdlog.h"
#include "spdlog/sinks/base_sink.h"
#define SPDLOG
#endif

typedef __int64 __fastcall MGS2_ActDashFireDelegate(__int64 a1);
typedef __int64 __fastcall MGS2_CreateDebrisTexDelegate(__int64 a1, float* a2, float* a3, unsigned int a4, int a5, int a6, float a7);
typedef __int64 __fastcall MGS2_GetTimeBaseDelegate(struct _exception* a1);

MGS2_ActDashFireDelegate* MGS2_ActDashFire;
MGS2_CreateDebrisTexDelegate* MGS2_CreateDebrisTex;
MGS2_GetTimeBaseDelegate* MGS2_GetTimeBase;

int* MGS2_CutsceneFlag = NULL; //Realtime demos / pad demos / forced camera views / FMV's / forced story codec calls
int* MGS2_RealtimeCutscene = NULL; //Realtime demos / pad demos / forced camera views
int MGS2_TimeBase = 5;
double MGS2_DashLastUpdate = 0;
double* MGS2_ActorWaitValue = NULL;

bool MGS2_IsCutsceneActive()
{
    return MGS2_CutsceneFlag == NULL ? false : (*MGS2_RealtimeCutscene == 1);
}

bool MGS2_IsRealtimeCutsceneActive()
{
    return MGS2_RealtimeCutscene == NULL ? false : (*MGS2_RealtimeCutscene == 1);
}

__int64 __fastcall MGS2_ActDashFireHook(__int64 a1)
{   
    if (!MGS2_IsCutsceneActive()) // ActDashFire() only needs to be slowed down during cutscenes, his boss fight runs at normal gamespeed.
        return MGS2_ActDashFire(a1);

    MGS2_DashLastUpdate += *MGS2_ActorWaitValue;

    if (MGS2_DashLastUpdate < 1.0 / 30.0) 
        return 0;

    MGS2_DashLastUpdate = 0;

    return MGS2_ActDashFire(a1);
}

__int64 __fastcall MGS2_CreateDebrisTexHook(__int64 a1, float* a2, float* a3, unsigned int a4, int a5, int a6, float a7)
{
    auto result = MGS2_CreateDebrisTex(a1, a2, a3, a4, a5, a6, a7);

    *(int*)(a1 + 100) = 15 * (MGS2_GetTimeBase(0) * ((Config.targetFramerate / 60) + 1)); // effect duration

    return result;
}

__int64 __fastcall MGS2_GetTimeBaseHook(struct _exception* a1)
{
    return MGS2_TimeBase;
}

bool MGS2_InitializeOffsets()
{
    MGS2_ActorWaitValue = (double*)(Memory::PatternScan(GameModule, "42 50 5F 4D 45 4D 4A 50 45 47") + 0x18);

    switch (Config.gameVersion) {
        case 0x1000200000000:
            MGS2_CutsceneFlag = (int*)(GameBase + 0xAADA64); //done
            MGS2_RealtimeCutscene = (int*)(GameBase + 0x16CE4B8); //done
            break;
        case 0x1000300000000:
            MGS2_CutsceneFlag = (int*)(GameBase + 0xAADA64); //done
            MGS2_RealtimeCutscene = (int*)(GameBase + 0x16CE4B8); //done
            break;
        case 0x1000400000000:
            MGS2_CutsceneFlag = (int*)(GameBase + 0xAB4C74); //done
            MGS2_RealtimeCutscene = (int*)(GameBase + 0x16D5638); //done
            break;
        case 0x1000400010000:
            MGS2_CutsceneFlag = (int*)(GameBase + 0xAB4C74); //done
            MGS2_RealtimeCutscene = (int*)(GameBase + 0x16D5638); //done
            break;
        case 0x1000500010000:
            MGS2_CutsceneFlag = (int*)(GameBase + 0xA8CD74); //done
            MGS2_RealtimeCutscene = (int*)(GameBase + 0x16AD878); //done
            break;
        case 0x2000000000000:
            MGS2_CutsceneFlag = (int*)(GameBase + 0xAA8F84);
            MGS2_RealtimeCutscene = (int*)(GameBase + 0x16CA5D8);
            break;
        case 0x2000000010000: 
            MGS2_CutsceneFlag = (int*)(GameBase + 0xAA9F94); //done
            MGS2_RealtimeCutscene = (int*)(GameBase + 0x16CB5D8); //done
            break;
        default:
            spdlog::error("MGS2: Unsupported game version!");
    }

    //logging
    MGS2_ActorWaitValue ? spdlog::info("MGS2: MGS2_InitializeOffsets() - MGS2_ActorWaitValue found: Offset is {:x}", (uintptr_t)MGS2_ActorWaitValue - (uintptr_t)GameModule) : spdlog::error("MGS2: MGS2_InitializeOffsets() failed to find MGS2_ActorWaitValue");
    MGS2_CutsceneFlag == NULL ? spdlog::error("MGS2: MGS2_InitializeOffsets() - failed to find MGS2_CutsceneFlag") : spdlog::info("MGS2: MGS2_InitializeOffsets() - MGS2_CutsceneFlag found.");
    MGS2_RealtimeCutscene == NULL ? spdlog::error("MGS2: MGS2_InitializeOffsets() - failed to find MGS2_RealtimeCutscene") : spdlog::info("MGS2: MGS2_InitializeOffsets() - MGS2_RealtimeCutscene found.");


    DWORD oldProtect;
    VirtualProtect(MGS2_ActorWaitValue, 8, PAGE_READWRITE, &oldProtect);

    return true;
}

void MGS2_InstallHooks()
{
    int status = MH_Initialize();

    uintptr_t actDashFireOffset = (uintptr_t)Memory::PatternScan(GameModule, "?? ?? ?? ?? ?? 49 8D AB 68 FE FF FF 48 81 EC 88");
    uintptr_t createDebrisTexOffset = (uintptr_t)Memory::PatternScan(GameModule, "40 55 53 56 57 41 54 41 56 41 57 48 8D AC 24 00");
    uintptr_t getTimeBaseOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 83 EC 28 E8 ?? ?? ?? ?? 33 C9 83 F8 01 0F 94");
    
    //logging
    actDashFireOffset ? spdlog::info("MGS2: InstallHooks() - actDashFireOffset() found: Offset is {:x}", (uintptr_t)actDashFireOffset - (uintptr_t)GameModule) : spdlog::error("MGS2: InstallHooks() failed to find actDashFireOffset");
    createDebrisTexOffset ? spdlog::info("MGS2: InstallHooks() - createDebrisTexOffset() found: Offset is {:x}", (uintptr_t)createDebrisTexOffset - (uintptr_t)GameModule) : spdlog::error("MGS2: InstallHooks() failed to find createDebrisTexOffset");
    getTimeBaseOffset ? spdlog::info("MGS2: InstallHooks() - getTimeBaseOffset() found: Offset is {:x}", (uintptr_t)getTimeBaseOffset - (uintptr_t)GameModule) : spdlog::error("MGS2: InstallHooks() failed to find getTimeBaseOffset");


    Memory::DetourFunction(actDashFireOffset, (LPVOID)MGS2_ActDashFireHook, (LPVOID*)&MGS2_ActDashFire);
    Memory::DetourFunction(createDebrisTexOffset, (LPVOID)MGS2_CreateDebrisTexHook, (LPVOID*)&MGS2_CreateDebrisTex);
    Memory::DetourFunction(getTimeBaseOffset, (LPVOID)MGS2_GetTimeBaseHook, (LPVOID*)&MGS2_GetTimeBase);
}

void MGS2_Init()
{
    MGS2_InitializeOffsets();
    MGS2_InstallHooks();

    while (true)
    {
        if (MGS2_ActorWaitValue != NULL)
        {
            if (MGS2_IsCutsceneActive())
            {
                MGS2_TimeBase = 5;
                *MGS2_ActorWaitValue = 1.0 / 60.0;
            }
            else
            {
                auto mod = (float)Config.targetFramerate / 60.0f;
                MGS2_TimeBase = std::round(5.0f / mod - 0.1);
                *MGS2_ActorWaitValue = 1.0 / (double)Config.targetFramerate;
            }
        }
        Sleep(1);
    }
}