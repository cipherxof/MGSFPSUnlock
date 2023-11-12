#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "ini.h"

HMODULE GameModule = GetModuleHandleA("METAL GEAR SOLID3.exe");
uintptr_t GameBase = (uintptr_t)GameModule;
int GameVersion = 0;
int TimeBase = 5;
int ConfigTargetFramerate = 60;
int* GV_TimeBase = NULL;
float* GV_Unknown = NULL; // animation blending value?
float* CameraSpeedModifier = NULL;
int* pInCutscene = NULL;
double* ActorWaitValue = NULL;

mINI::INIStructure Config;

bool InCutscene() // todo: find a more accurate method
{
    return pInCutscene == NULL ? false : *pInCutscene == 0;
}

typedef uint64_t __fastcall GetTimeBaseDelegate(struct _exception* a1);
GetTimeBaseDelegate* GetTimeBase;
uint64_t __fastcall GetTimeBaseHook(struct _exception* a1)
{
    if (InCutscene())
        return 5;

    return TimeBase;
}

float CalculateMotionTimeBase(float value)
{
    if (value <= 0.00f)
        value = (float)*GV_TimeBase;

    value = value * (5.0f / (float)*GV_TimeBase);

    return value / ((float)ConfigTargetFramerate / 60.0f);
}

typedef void __fastcall UpdateMotionTimeBaseADelegate(__int64 a1, float a2, int a3);
UpdateMotionTimeBaseADelegate* UpdateMotionTimeBaseA;
void __fastcall UpdateMotionTimeBaseAHook(__int64 a1, float a2, int a3)
{
    UpdateMotionTimeBaseA(a1, CalculateMotionTimeBase(a2), a3);
}

typedef void __fastcall UpdateMotionTimeBaseBDelegate(__int64 a1, float a2);
UpdateMotionTimeBaseBDelegate* UpdateMotionTimeBaseB;
void __fastcall UpdateMotionTimeBaseBHook(__int64 a1, float a2)
{
    UpdateMotionTimeBaseB(a1, CalculateMotionTimeBase(a2));
}

void InstallHooks() 
{
    int status = MH_Initialize();

    uintptr_t getTimeBaseOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 83 EC 28 E8 97 E6 FD FF 33 C9 83 F8 01 0F 94");
    uintptr_t updateMotionAOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 85 C9 74 31 0F 57 C0 0F 2F C1 76 0B 66 0F 6E");
    uintptr_t updateMotionBOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 85 C9 74 3F 0F 57 C0 0F 2F C1 76 0B 66 0F 6E");

    //48 83 EC 38 0F 29 74 24  20 0F 28 F0 E8 6F E7 FD
    Memory::DetourFunction(getTimeBaseOffset, (LPVOID)GetTimeBaseHook, (LPVOID*)&GetTimeBase);
    Memory::DetourFunction(updateMotionAOffset, (LPVOID)UpdateMotionTimeBaseAHook, (LPVOID*)&UpdateMotionTimeBaseA);
    Memory::DetourFunction(updateMotionBOffset, (LPVOID)UpdateMotionTimeBaseBHook, (LPVOID*)&UpdateMotionTimeBaseB);
}

bool Initialize()
{
    switch (GameVersion)
    {
    case 0x00010000:
        // not supported
        return false;
    case 0x00010002:
        GV_TimeBase = (int*)(GameBase + 0x1D8D8B0);
        GV_Unknown = (float*)(GameBase + 0x8F195C);
        ActorWaitValue = (double*)(GameBase + 0x8EBF40);
        pInCutscene = (int*)(GameBase + 0x1E41570);
        CameraSpeedModifier = (float*)(GameBase + 0x8EC2C4);
        break;
    default:
        return false;
    }

    if (ConfigTargetFramerate <= 0)
        return false;

    DWORD oldProtect;
    VirtualProtect(ActorWaitValue, 8, PAGE_READWRITE, &oldProtect);
    VirtualProtect(CameraSpeedModifier, 4, PAGE_READWRITE, &oldProtect);
    VirtualProtect(GV_Unknown, 4, PAGE_READWRITE, &oldProtect);

    float value = (float)ConfigTargetFramerate / 60.0f;
    TimeBase = std::round(5.0f / value - 0.1);
    *CameraSpeedModifier = 30.0f / value;
    *GV_TimeBase = TimeBase;
    *GV_Unknown = *GV_Unknown * value;
    *ActorWaitValue = 1.0 / (double)ConfigTargetFramerate;

    return true;
}

void ReadConfig()
{
    mINI::INIFile file("MGSFPSUnlock.ini");
    file.read(Config);
    ConfigTargetFramerate = std::stoi(Config["Settings"]["TargetFrameRate"]);
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
    GameVersion = GetGameVersion(GameModule);

    if (GameVersion == 0)
        return false;

    Sleep(3000); // delay, just in case
    ReadConfig();
    if (!Initialize())
        return false;
    InstallHooks();

    while (true)
    {
        if (ActorWaitValue != NULL)
        {
            if (InCutscene())
            {
                *GV_TimeBase = 5;
                *ActorWaitValue = 1.0 / 60.0;
            }
            else
            {
                *GV_TimeBase = TimeBase;
                *ActorWaitValue = 1.0 / (double)ConfigTargetFramerate;
            }
        }
        Sleep(16);
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

