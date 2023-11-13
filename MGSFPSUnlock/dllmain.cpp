#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "ini.h"

HMODULE GameModule = GetModuleHandleA("METAL GEAR SOLID3.exe");
uintptr_t GameBase = (uintptr_t)GameModule;
mINI::INIStructure Config;
int GameVersion = 0;
int TimeBase = 5;
int ConfigTargetFramerate = 60;
float FrameRateModifier = 1.0f;
int* GV_TimeBase = NULL;
int* CutsceneFlag = NULL;
double* ActorWaitValue = NULL;

typedef uint64_t __fastcall GetTimeBaseDelegate(struct _exception* a1);
typedef void __fastcall UpdateMotionTimeBaseADelegate(uint64_t a1, float a2, int a3);
typedef void __fastcall UpdateMotionTimeBaseBDelegate(uint64_t a1, float a2);
typedef int64_t __fastcall GetTargetFpsDelegate(struct _exception* a1);
typedef int64_t __fastcall ThrowItemDelegate(uint64_t a1, int* a2, float* a3, int a4, uint16_t a5, int a6, int a7, int a8, struct _exception* a9, int a10);
typedef int16_t __fastcall UpdateAnimationBlendingDelegate(struct _exception* a1, int16_t a2);

GetTimeBaseDelegate* GetTimeBase;
UpdateMotionTimeBaseADelegate* UpdateMotionTimeBaseA;
UpdateMotionTimeBaseBDelegate* UpdateMotionTimeBaseB;
GetTargetFpsDelegate* GetTargetFps;
ThrowItemDelegate* ThrowItem;
UpdateAnimationBlendingDelegate* UpdateAnimationBlending;

bool InCutscene() // todo: find a more accurate method
{
    return CutsceneFlag == NULL ? false : *CutsceneFlag == 0;
}

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

    return value / FrameRateModifier;
}

void __fastcall UpdateMotionTimeBaseAHook(uint64_t a1, float a2, int a3)
{
    UpdateMotionTimeBaseA(a1, CalculateMotionTimeBase(a2), a3);
}

void __fastcall UpdateMotionTimeBaseBHook(uint64_t a1, float a2)
{
    UpdateMotionTimeBaseB(a1, CalculateMotionTimeBase(a2));
}

int64_t __fastcall GetTargetFpsHook(struct _exception* a1)
{
    return ConfigTargetFramerate;
}

int64_t __fastcall ThrowItemHook(uint64_t a1, int* a2, float* a3, int a4, uint16_t a5, int a6, int a7, int a8, struct _exception* a9, int a10)
{
    *(float*)&a6 = *(float*)&a6 / FrameRateModifier;

    return ThrowItem(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
}

int16_t UpdateAnimationBlendingHook(int16_t currentFrame, int16_t targetFrame)
{
    const float smoothingFactor = 8.0f * FrameRateModifier;

    int16_t diff = targetFrame - currentFrame;
    int16_t value = (float)(diff) / smoothingFactor;

    if (*GV_TimeBase != GetTimeBase(NULL))
        currentFrame = (value != 0) ? (currentFrame + value) : targetFrame;

    return (value != 0) ? (currentFrame + value) : targetFrame;
}

void InstallHooks() 
{
    int status = MH_Initialize();

    uintptr_t getTimeBaseOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 83 EC 28 E8 97 E6 FD FF 33 C9 83 F8 01 0F 94");
    uintptr_t updateMotionAOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 85 C9 74 31 0F 57 C0 0F 2F C1 76 0B 66 0F 6E");
    uintptr_t updateMotionBOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 85 C9 74 3F 0F 57 C0 0F 2F C1 76 0B 66 0F 6E");
    uintptr_t getTargetFpsOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 83 EC 28 E8 77 E6 FD FF 83 F8 01 B9 3C 00 00");
    uintptr_t throwItemOffset = (uintptr_t)Memory::PatternScan(GameModule, "40 55 56 57 41 56 41 57 48 8D 6C 24 E0 48 81 EC");

    Memory::DetourFunction(getTimeBaseOffset, (LPVOID)GetTimeBaseHook, (LPVOID*)&GetTimeBase);
    Memory::DetourFunction(updateMotionAOffset, (LPVOID)UpdateMotionTimeBaseAHook, (LPVOID*)&UpdateMotionTimeBaseA);
    Memory::DetourFunction(updateMotionBOffset, (LPVOID)UpdateMotionTimeBaseBHook, (LPVOID*)&UpdateMotionTimeBaseB);
    Memory::DetourFunction(getTargetFpsOffset, (LPVOID)GetTargetFpsHook, (LPVOID*)&GetTargetFps);
    Memory::DetourFunction(throwItemOffset, (LPVOID)ThrowItemHook, (LPVOID*)&ThrowItem);
    Memory::DetourFunction(GameBase + 0x94CD0, (LPVOID)UpdateAnimationBlendingHook, (LPVOID*)&UpdateAnimationBlending);
}

bool Initialize()
{
    if (ConfigTargetFramerate <= 0)
        return false;

    float* cameraSpeedModifierA = NULL;
    float* cameraSpeedModifierB = NULL;
    float* realTimeRate = NULL;

    switch (GameVersion)
    {
    case 0x00010000:
        // not supported
        return false;
    case 0x00010002:
        GV_TimeBase = (int*)(GameBase + 0x1D8D8B0);
        ActorWaitValue = (double*)(GameBase + 0x8EBF40);
        CutsceneFlag = (int*)(GameBase + 0x1E41570);
        cameraSpeedModifierA = (float*)(GameBase + 0x8EC2C4);
        cameraSpeedModifierB = (float*)(GameBase + 0x910BCC);
        realTimeRate = (float*)(GameBase + 0x8F18E0);
        break;
    default:
        return false;
    }

    DWORD oldProtect;
    VirtualProtect(ActorWaitValue, 8, PAGE_READWRITE, &oldProtect);
    VirtualProtect(cameraSpeedModifierA, 4, PAGE_READWRITE, &oldProtect);
    VirtualProtect(cameraSpeedModifierB, 4, PAGE_READWRITE, &oldProtect);
    VirtualProtect(realTimeRate, 4, PAGE_READWRITE, &oldProtect);

    FrameRateModifier = (float)ConfigTargetFramerate / 60.0f;
    TimeBase = std::round(5.0f / FrameRateModifier - 0.1);
    *GV_TimeBase = TimeBase;
    *ActorWaitValue = 1.0 / (double)ConfigTargetFramerate;
    *cameraSpeedModifierA = *cameraSpeedModifierA / FrameRateModifier;
    *cameraSpeedModifierB = *cameraSpeedModifierB / FrameRateModifier;
    *realTimeRate = *realTimeRate / FrameRateModifier;

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
        Sleep(1);
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

