#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "ini.h"
#include "config.h"

typedef __int64 __fastcall MGS2_ActDashFireDelegate(__int64 a1);
typedef __int64 __fastcall MGS2_CreateDebrisTexDelegate(__int64 a1, float* a2, float* a3, unsigned int a4, int a5, int a6, float a7);
typedef __int64 __fastcall MGS2_GetTimeBaseDelegate(struct _exception* a1);

MGS2_ActDashFireDelegate* MGS2_ActDashFire;
MGS2_CreateDebrisTexDelegate* MGS2_CreateDebrisTex;
MGS2_GetTimeBaseDelegate* MGS2_GetTimeBase;

int MGS2_TimeBase = 5;
double MGS2_DashLastUpdate = 0;
double* MGS2_ActorWaitValue = NULL;

bool MGS2_InCutscene()
{
    return false;
}

__int64 __fastcall MGS2_ActDashFireHook(__int64 a1)
{
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
            if (MGS2_InCutscene())
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