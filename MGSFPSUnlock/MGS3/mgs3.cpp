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

int TimeBase = 5;
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

bool InCutscene()
{
    return CutsceneFlag == NULL ? false : (*CutsceneFlag == 0);
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
    return Config.targetFramerate;
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

bool MGS3_InitializeOffsets()
{
    float* cameraSpeedModifierA = NULL;
    float* cameraSpeedModifierB = NULL;
    float* realTimeRate = NULL;

    switch (Config.gameVersion)
    {
    case 0x1000200000000:
        GV_TimeBase = (int*)(GameBase + 0x1D8D8B0);
        ActorWaitValue = (double*)(GameBase + 0x8EBF40);
        CutsceneFlag = (int*)(GameBase + 0x1E41570);
        cameraSpeedModifierA = (float*)(GameBase + 0x8EC2C4);
        cameraSpeedModifierB = (float*)(GameBase + 0x910BCC);
        realTimeRate = (float*)(GameBase + 0x8F18E0);
        break;
    case 0x1000300000000:
        GV_TimeBase = (int*)(GameBase + 0x1D8E8A0);
        ActorWaitValue = (double*)(GameBase + 0x8EBF48);
        CutsceneFlag = (int*)(GameBase + 0x1E42570);
        cameraSpeedModifierA = (float*)(GameBase + 0x8EC2CC);
        cameraSpeedModifierB = (float*)(GameBase + 0x910F80);
        realTimeRate = (float*)(GameBase + 0x8F18E0);
        break;
    case 0x1000400000000:
        GV_TimeBase = (int*)(GameBase + 0x1D97D10);
        ActorWaitValue = (double*)(GameBase + 0x8F2DE8);
        CutsceneFlag = (int*)(GameBase + 0x1E4BA30);
        cameraSpeedModifierA = (float*)(GameBase + 0x8F24B4);
        cameraSpeedModifierB = (float*)(GameBase + 0x9186A0);
        realTimeRate = (float*)(GameBase + 0x8F90A0);
        break;
    case 0x1000400010000:
        GV_TimeBase = (int*)(GameBase + 0x1D97D10);
        ActorWaitValue = (double*)(GameBase + 0x8F2DF8);
        CutsceneFlag = (int*)(GameBase + 0x1E4BA50);
        cameraSpeedModifierA = (float*)(GameBase + 0x8F24C4);
        cameraSpeedModifierB = (float*)(GameBase + 0x9186E0);
        realTimeRate = (float*)(GameBase + 0x8F90E0);
        break;
    case 0x1000500010000:
        GV_TimeBase = (int*)(GameBase + 0x1D97D10);
        ActorWaitValue = (double*)(GameBase + 0x8F2DF8);
        CutsceneFlag = (int*)(GameBase + 0x1E4BA50);
        cameraSpeedModifierA = (float*)(GameBase + 0x8F24C4);
        cameraSpeedModifierB = (float*)(GameBase + 0x9186E0);
        realTimeRate = (float*)(GameBase + 0x8F90E0);
        break;
    case 0x2000000000000:
        GV_TimeBase = (int*)(GameBase + 0x1D97D10);
        ActorWaitValue = (double*)(GameBase + 0x8F2DF8);
        CutsceneFlag = (int*)(GameBase + 0x1E4BA50);
        cameraSpeedModifierA = (float*)(GameBase + 0x8F24C4);
        cameraSpeedModifierB = (float*)(GameBase + 0x9186E0);
        realTimeRate = (float*)(GameBase + 0x8F90E0);
        break;
    case 0x2000000010000:  //done
        GV_TimeBase = (int*)(GameBase + 0x1D77740);
        ActorWaitValue = (double*)(GameBase + 0x8B7C08);
        CutsceneFlag = (int*)(GameBase + 0x1E2B530); 
        cameraSpeedModifierA = (float*)(GameBase + 0x8B71C0); 
        cameraSpeedModifierB = (float*)(GameBase + 0x8F0790); 
        realTimeRate = (float*)(GameBase + 0x8D0F70); 
        break;
    default:
        spdlog::error("MGS3: Unsupported game version!");
        return false;
    }

    //logging
    GV_TimeBase == NULL ? spdlog::info("MGS3: MGS3_InitializeOffsets() - failed to find GV_TimeBase") : spdlog::info("MGS3: MGS3_InitializeOffsets() - GV_TimeBase found.");
    ActorWaitValue == NULL ? spdlog::info("MGS3: MGS3_InitializeOffsets() - failed to find ActorWaitValue") : spdlog::info("MGS3: MGS3_InitializeOffsets() - ActorWaitValue found.");
    CutsceneFlag == NULL ? spdlog::info("MGS3: MGS3_InitializeOffsets() - failed to find CutsceneFlag") : spdlog::info("MGS3: MGS3_InitializeOffsets() - CutsceneFlag found.");
    cameraSpeedModifierA == NULL ? spdlog::info("MGS3: MGS3_InitializeOffsets() - failed to find cameraSpeedModifierA") : spdlog::info("MGS3: MGS3_InitializeOffsets() - cameraSpeedModifierA found.");
    cameraSpeedModifierB == NULL ? spdlog::info("MGS3: MGS3_InitializeOffsets() - failed to find cameraSpeedModifierB") : spdlog::info("MGS3: MGS3_InitializeOffsets() - cameraSpeedModifierB found.");
    realTimeRate == NULL ? spdlog::info("MGS3: MGS3_InitializeOffsets() - failed to find realTimeRate") : spdlog::info("MGS3: MGS3_InitializeOffsets() - realTimeRate found.");

    DWORD oldProtect;
    VirtualProtect(ActorWaitValue, 8, PAGE_READWRITE, &oldProtect);
    VirtualProtect(cameraSpeedModifierA, 4, PAGE_READWRITE, &oldProtect);
    VirtualProtect(cameraSpeedModifierB, 4, PAGE_READWRITE, &oldProtect);
    VirtualProtect(realTimeRate, 4, PAGE_READWRITE, &oldProtect);

    FrameRateModifier = (float)Config.targetFramerate / 60.0f;
    TimeBase = std::round(5.0f / FrameRateModifier - 0.1);
    *GV_TimeBase = TimeBase;
    *ActorWaitValue = 1.0 / (double)Config.targetFramerate;
    *cameraSpeedModifierA = *cameraSpeedModifierA / FrameRateModifier;
    *cameraSpeedModifierB = *cameraSpeedModifierB / FrameRateModifier;
    *realTimeRate = *realTimeRate / FrameRateModifier;

    return true;
}

void MGS3_InstallHooks()
{
    int status = MH_Initialize();

    uintptr_t getTimeBaseOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 83 EC 28 E8 ?? ?? FD FF 33 C9 83 F8 01 0F 94");
    uintptr_t updateMotionAOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 85 C9 74 31 0F 57 C0 0F 2F C1 76 0B 66 0F 6E");
    uintptr_t updateMotionBOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 85 C9 74 3F 0F 57 C0 0F 2F C1 76 0B 66 0F 6E");
    uintptr_t getTargetFpsOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 83 EC 28 E8 ?? ?? FD FF 83 F8 01 B9 3C 00 00");
    uintptr_t throwItemOffset = (uintptr_t)Memory::PatternScan(GameModule, "40 55 56 57 41 56 41 57 48 8D 6C 24 E0 48 81 EC");
    uintptr_t animBlendOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 89 5C 24 08 57 48 83 EC 30 0F 29 74 24 20 0F");

    //logging
    getTimeBaseOffset ? spdlog::info("MGS3: InstallHooks() - getTimeBaseOffset() found: Offset is {:x}", (uintptr_t)getTimeBaseOffset - (uintptr_t)GameModule) : spdlog::error("MGS3: InstallHooks() failed to find getTimeBaseOffset");
    updateMotionAOffset ? spdlog::info("MGS3: InstallHooks() - updateMotionAOffset() found: Offset is {:x}", (uintptr_t)updateMotionAOffset - (uintptr_t)GameModule) : spdlog::error("MGS3: InstallHooks() failed to find updateMotionAOffset");
    updateMotionBOffset ? spdlog::info("MGS3: InstallHooks() - updateMotionBOffset() found: Offset is {:x}", (uintptr_t)updateMotionBOffset - (uintptr_t)GameModule) : spdlog::error("MGS3: InstallHooks() failed to find updateMotionBOffset");
    getTargetFpsOffset ? spdlog::info("MGS3: InstallHooks() - getTargetFpsOffset() found: Offset is {:x}", (uintptr_t)getTargetFpsOffset - (uintptr_t)GameModule) : spdlog::error("MGS3: InstallHooks() failed to find getTargetFpsOffset");
    throwItemOffset ? spdlog::info("MGS3: InstallHooks() - throwItemOffset() found: Offset is {:x}", (uintptr_t)throwItemOffset - (uintptr_t)GameModule) : spdlog::error("MGS3: InstallHooks() failed to find throwItemOffset");
    animBlendOffset ? spdlog::info("MGS3: InstallHooks() - animBlendOffset() found: Offset is {:x}", (uintptr_t)animBlendOffset - (uintptr_t)GameModule) : spdlog::error("MGS3: InstallHooks() failed to find animBlendOffset");

    Memory::DetourFunction(getTimeBaseOffset, (LPVOID)GetTimeBaseHook, (LPVOID*)&GetTimeBase);
    Memory::DetourFunction(updateMotionAOffset, (LPVOID)UpdateMotionTimeBaseAHook, (LPVOID*)&UpdateMotionTimeBaseA);
    Memory::DetourFunction(updateMotionBOffset, (LPVOID)UpdateMotionTimeBaseBHook, (LPVOID*)&UpdateMotionTimeBaseB);
    Memory::DetourFunction(getTargetFpsOffset, (LPVOID)GetTargetFpsHook, (LPVOID*)&GetTargetFps);
    Memory::DetourFunction(throwItemOffset, (LPVOID)ThrowItemHook, (LPVOID*)&ThrowItem);
    Memory::DetourFunction(animBlendOffset, (LPVOID)UpdateAnimationBlendingHook, (LPVOID*)&UpdateAnimationBlending);
}

void MGS3_Init()
{
    if (!MGS3_InitializeOffsets())
        return;

    MGS3_InstallHooks();
    
    spdlog::info("MGS3: Intiailization successful!");

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
                *ActorWaitValue = 1.0 / (double)Config.targetFramerate;
            }
        }
        Sleep(1);
    }
}