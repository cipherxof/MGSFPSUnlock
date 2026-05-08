#include "Memory.h"
#include "MinHook.h"
#include "config.h"
#include "spdlog/spdlog.h"
#include <Utils.h>

class MGS3FramerateUnlocker 
{
public:
    MGS3FramerateUnlocker();
    ~MGS3FramerateUnlocker();

    bool Initialize();

private:
    static MGS3FramerateUnlocker* instance;

    // Constants
    static constexpr int DEFAULT_TIMEBASE = 5;
    static constexpr float DEFAULT_FPS = 60.0f;
    static constexpr float SMOOTHING_FACTOR = 8.0f;

    // Function delegates
    typedef uint64_t(__fastcall* GetTimeBaseDelegate)(struct _exception* a1);
    typedef void(__fastcall* UpdateMotionTimeBaseADelegate)(uint64_t a1, float a2, int a3);
    typedef void(__fastcall* UpdateMotionTimeBaseBDelegate)(uint64_t a1, float a2);
    typedef int64_t(__fastcall* GetTargetFpsDelegate)(struct _exception* a1);
    typedef int64_t(__fastcall* ThrowItemDelegate)(uint64_t a1, int* a2, float* a3, int a4, uint16_t a5, int a6, int a7, int a8, struct _exception* a9, int a10);
    typedef int16_t(__fastcall* UpdateAnimationBlendingDelegate)(struct _exception* a1, int16_t a2);
    typedef double(*GetActorExecTimeDelegate)();
    typedef __int64(__fastcall* AdjustTickDelegate)(struct _exception* a1);
    typedef __int64(__fastcall* UpdateTitleScreenLayerDelegate)(__int64 a1);
    typedef __int64(__fastcall* SetTitleScreenPlaybackSpeedDelegate)(__int64 a1, float a2);

    // Game data structure
    struct GameVariables 
    {
        int* timeBase;
        double* actorWaitValue;
        int* cutsceneFlag;
        float* cameraSpeedModifierA;
        float* cameraSpeedModifierB;
        float* realTimeRate;
    };

    //  function pointers
    GetTimeBaseDelegate GetTimeBase;
    UpdateMotionTimeBaseADelegate UpdateMotionTimeBaseA;
    UpdateMotionTimeBaseBDelegate UpdateMotionTimeBaseB;
    GetTargetFpsDelegate GetTargetFps;
    ThrowItemDelegate ThrowItem;
    UpdateAnimationBlendingDelegate UpdateAnimationBlending;
    GetActorExecTimeDelegate GetActorExecTime;
    AdjustTickDelegate AdjustTick;
    AdjustTickDelegate AdjustTick2;
    AdjustTickDelegate AdjustTick3;
    AdjustTickDelegate AdjustTick5;
    AdjustTickDelegate ConvertFrom60;
    AdjustTickDelegate ConvertTo60;
    UpdateTitleScreenLayerDelegate UpdateTitleScreenLayer;
    SetTitleScreenPlaybackSpeedDelegate SetTitleScreenPlaybackSpeed;

    // Game data
    GameVariables gameVars;
    float frameRateModifier;
    int timeBase;
    float timeBaseFloat = (float)DEFAULT_TIMEBASE;
    double execTime = 0.00;

    // Private methods
    bool InitializeOffsets();
    bool InstallHooks();
    bool InCutscene() const;
    float CalculateMotionTimeBase(float value) const;

    // Hook methods
    static uint64_t __fastcall GetTimeBaseHook(struct _exception* a1);
    static void __fastcall UpdateMotionTimeBaseAHook(uint64_t a1, float a2, int a3);
    static void __fastcall UpdateMotionTimeBaseBHook(uint64_t a1, float a2);
    static int64_t __fastcall GetTargetFpsHook(struct _exception* a1);
    static int64_t __fastcall ThrowItemHook(uint64_t a1, int* a2, float* a3, int a4, uint16_t a5, int a6, int a7, int a8, struct _exception* a9, int a10);
    static int16_t __fastcall UpdateAnimationBlendingHook(int16_t currentFrame, int16_t targetFrame);
    static double GetActorExecTimeHook();
    static __int64 __fastcall AdjustTickHook(struct _exception* a1);
    static __int64 __fastcall AdjustTick2Hook(struct _exception* a1);
    static float __fastcall AdjustTick3Hook(float tick);
    static float __fastcall AdjustTick5Hook(float tick);
    static __int64 __fastcall ConvertFrom60Hook(struct _exception* a1);
    static __int64 __fastcall ConvertTo60Hook(struct _exception* a1);
    static __int64 __fastcall SetTitleScreenPlaybackSpeedHook(__int64 a1, float a2);
    static __int64 __fastcall UpdateTitleScreenLayerHook(__int64 a1);
};

MGS3FramerateUnlocker* MGS3FramerateUnlocker::instance = nullptr;

MGS3FramerateUnlocker::MGS3FramerateUnlocker()
    : frameRateModifier(1.0f), timeBase(DEFAULT_TIMEBASE)
{
    instance = this;
    memset(&gameVars, 0, sizeof(GameVariables));
}

MGS3FramerateUnlocker::~MGS3FramerateUnlocker()
{
    instance = nullptr;
}

bool MGS3FramerateUnlocker::InitializeOffsets()
{
    spdlog::info("Initializing offsets...");
    
    uint8_t* addr = nullptr;
    memset(&gameVars, 0, sizeof(GameVariables));

    addr = Memory::PatternScan(GameModule, "33 C9 83 F8 01 ?? ?? C1 83 C1 05");
    if (!addr) spdlog::error("Could not find GV_TimeBase");
    else gameVars.timeBase = reinterpret_cast<int*>(GetRelativeOffset(addr + 13));

    addr = Memory::PatternScan(GameModule, "83 3D ?? ?? ?? ?? 00 ?? ?? F2 0F 10 0D");
    if (!addr) spdlog::error("Could not find ActorWaitValue");
    else gameVars.actorWaitValue = reinterpret_cast<double*>(GetRelativeOffset(addr + 13));

    addr = Memory::PatternScan(GameModule, "89 2D ?? ?? ?? ?? 89 2D ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 40 88 2D");
    if (!addr) spdlog::error("Could not find CutsceneFlag");
    else gameVars.cutsceneFlag = reinterpret_cast<int*>(GetRelativeOffset(addr + 2));

    addr = Memory::PatternScan(GameModule, "0F 28 F0 E8 ?? ?? ?? ?? F3 0F 10 0D ?? ?? ?? ??");
    if (!addr) spdlog::error("Could not find CameraSpeedModifierA");
    else gameVars.cameraSpeedModifierA = reinterpret_cast<float*>(GetRelativeOffset(addr + 12));

    addr = Memory::PatternScan(GameModule, "0F 28 ?? F3 0F 10 87 90 00 00 00 F3 0F 10 15");
    if (!addr) spdlog::error("Could not find CameraSpeedModifierB");
    else gameVars.cameraSpeedModifierB = reinterpret_cast<float*>(GetRelativeOffset(addr + 15));

    addr = Memory::PatternScan(GameModule, "0F 6E C0 0F 5B C0 F3 0F 59 05 ?? ?? ?? ?? F3 48 0F 2C C0", 1);
    if (!addr) spdlog::error("Could not find RealTimeRate");
    else gameVars.realTimeRate = reinterpret_cast<float*>(GetRelativeOffset(addr + 10));

    // set offsets manually here (useful for development)
    switch (Config.gameVersion) 
    {
    case 0x2000000010000:
        break;
    default:
        break;
    }

    LogAddress("timeBase", (uintptr_t)gameVars.timeBase);
    LogAddress("actorWaitValue", (uintptr_t)gameVars.actorWaitValue);
    LogAddress("cutsceneFlag", (uintptr_t)gameVars.cutsceneFlag);
    LogAddress("cameraSpeedModifierA", (uintptr_t)gameVars.cameraSpeedModifierA);
    LogAddress("cameraSpeedModifierB", (uintptr_t)gameVars.cameraSpeedModifierB);
    LogAddress("realTimeRate", (uintptr_t)gameVars.realTimeRate);

    if (!gameVars.timeBase || !gameVars.actorWaitValue || !gameVars.cutsceneFlag || !gameVars.cameraSpeedModifierA ||
        !gameVars.cameraSpeedModifierB || !gameVars.realTimeRate) 
    {
        spdlog::error("Failed to initialize one or more critical offsets");
        return false;
    }
    
    DWORD oldProtect;
    VirtualProtect(gameVars.actorWaitValue, 8, PAGE_READWRITE, &oldProtect);
    VirtualProtect(gameVars.cameraSpeedModifierA, 4, PAGE_READWRITE, &oldProtect);
    VirtualProtect(gameVars.cameraSpeedModifierB, 4, PAGE_READWRITE, &oldProtect);
    VirtualProtect(gameVars.realTimeRate, 4, PAGE_READWRITE, &oldProtect);

    return true;
}

bool MGS3FramerateUnlocker::InstallHooks()
{
    spdlog::info("Installing hooks");

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) 
    {
        spdlog::error("Failed to initialize MinHook, status: {}", (int)status);
        return false;
    }

    uint8_t* getTimeBase = Memory::PatternScan(GameModule, "48 83 EC 28 E8 ?? ?? FD FF 33 C9 83 F8 01 0F 94");
    uint8_t* updateMotionA = Memory::PatternScan(GameModule, "48 85 C9 74 31 0F 57 C0 0F 2F C1 76 0B 66 0F 6E");
    uint8_t* updateMotionB = Memory::PatternScan(GameModule, "48 85 C9 74 3F 0F 57 C0 0F 2F C1 76 0B 66 0F 6E");
    uint8_t* getTargetFps = Memory::PatternScan(GameModule, "48 83 EC 28 E8 ?? ?? FD FF 83 F8 01 B9 3C 00 00");
    uint8_t* throwItem = Memory::PatternScan(GameModule, "40 55 56 57 41 56 41 57 48 8D 6C 24 E0 48 81 EC");
    uint8_t* animBlend = Memory::PatternScan(GameModule, "48 89 5C 24 08 57 48 83 EC 30 0F 29 74 24 20 0F");
    uint8_t* getActorExecTime = Memory::PatternScan(GameModule, "48 83 EC 28 48 8D 0D ?? ?? ?? 01 FF 15 ?? ?? 78");
    uint8_t* adjustTick = Memory::PatternScan(GameModule, "40 53 48 83 EC ?? 8B D9 E8 ?? ?? ?? ?? 83 F8 ?? 75 ?? 8D 0C 9D");
    uint8_t* adjustTick2 = Memory::PatternScan(GameModule, "40 53 48 83 EC ?? 8B D9 E8 ?? ?? ?? ?? 83 F8 ?? 75 ?? 8D 0C 5B");
    uint8_t* adjustTick3 = Memory::PatternScan(GameModule, "48 83 EC ?? 0F 29 74 24 ?? 0F");
    uint8_t* adjustTick5 = Memory::PatternScan(GameModule, "48 83 EC ?? 0F 29 74 24 ?? 0F", 1);
    uint8_t* adjustTick8 = Memory::PatternScan(GameModule, "48 83 EC ?? 0F 29 74 24 ?? 0F", 2);
    uint8_t* adjustTick9 = Memory::PatternScan(GameModule, "48 83 EC ?? 0F 29 74 24 ?? 0F", 3);
    uint8_t* convertFrom60 = Memory::PatternScan(GameModule, "40 53 48 83 EC ?? 48 63 D9 E8 ?? ?? ?? ?? 85 C0 74 ?? 48 8D 0C 9B");
    uint8_t* convertTo60 = Memory::PatternScan(GameModule, "40 53 48 83 EC ?? 48 63 D9 E8 ?? ?? ?? ?? 85 C0 74 ?? 48 8D 0C 5B");
    uint8_t* setTitleScreenPlaybackSpeed = Memory::PatternScan(GameModule, "48 8B C1 48 C1 E0 ?? 48 85 C9 74 ?? 48 39 48 ?? 75 ? F3 0F 11 88 ?? ?? ?? ?? C3");
    uint8_t* updateTitleScreenLayer = Memory::PatternScan(GameModule, "48 89 5C 24 ?? 57 48 83 EC ?? 4C 8B 81 ?? ?? ?? ?? 0F 57 D2");

    LogAddress("getTimeBase", (uintptr_t)getTimeBase);
    LogAddress("updateMotionA", (uintptr_t)updateMotionA);
    LogAddress("updateMotionB", (uintptr_t)updateMotionB);
    LogAddress("getTargetFps", (uintptr_t)getTargetFps);
    LogAddress("throwItem", (uintptr_t)throwItem);
    LogAddress("animBlend", (uintptr_t)animBlend);
    LogAddress("getActorExecTime", (uintptr_t)getActorExecTime);
    LogAddress("adjustTick", (uintptr_t)adjustTick);
    LogAddress("adjustTick2", (uintptr_t)adjustTick2);
    LogAddress("adjustTick3", (uintptr_t)adjustTick3);
    LogAddress("adjustTick5", (uintptr_t)adjustTick5);
    LogAddress("adjustTick8", (uintptr_t)adjustTick8);
    LogAddress("adjustTick9", (uintptr_t)adjustTick9);
    LogAddress("convertFrom60", (uintptr_t)convertFrom60);
    LogAddress("convertTo60", (uintptr_t)convertTo60);
    LogAddress("setTitleScreenPlaybackSpeed", (uintptr_t)setTitleScreenPlaybackSpeed);
    LogAddress("updateTitleScreenLayer", (uintptr_t)updateTitleScreenLayer);

    if (!getTimeBase || !updateMotionA || !updateMotionB || !getTargetFps || !throwItem || !animBlend || !getActorExecTime ||
        !adjustTick || !adjustTick2 || !adjustTick3 || !adjustTick5 || !adjustTick8 || !adjustTick9 || !convertFrom60 || !convertTo60 ||
        !setTitleScreenPlaybackSpeed || !updateTitleScreenLayer)
    {
        spdlog::error("Failed to find one or more function patterns");
        return false;
    }

    MH_CreateHook(getTimeBase, GetTimeBaseHook, reinterpret_cast<void**>(&GetTimeBase));
    MH_CreateHook(updateMotionA, UpdateMotionTimeBaseAHook, reinterpret_cast<void**>(&UpdateMotionTimeBaseA));
    MH_CreateHook(updateMotionB, UpdateMotionTimeBaseBHook, reinterpret_cast<void**>(&UpdateMotionTimeBaseB));
    MH_CreateHook(getTargetFps, GetTargetFpsHook, reinterpret_cast<void**>(&GetTargetFps));
    MH_CreateHook(throwItem, ThrowItemHook, reinterpret_cast<void**>(&ThrowItem));
    MH_CreateHook(animBlend, UpdateAnimationBlendingHook, reinterpret_cast<void**>(&UpdateAnimationBlending));
    MH_CreateHook(getActorExecTime, GetActorExecTimeHook, reinterpret_cast<void**>(&GetActorExecTime));
    MH_CreateHook(adjustTick, AdjustTickHook, reinterpret_cast<void**>(&AdjustTick));
    MH_CreateHook(adjustTick2, AdjustTick2Hook, reinterpret_cast<void**>(&AdjustTick2));
    MH_CreateHook(adjustTick3, AdjustTick3Hook, reinterpret_cast<void**>(&AdjustTick3));
    MH_CreateHook(adjustTick5, AdjustTick5Hook, reinterpret_cast<void**>(&AdjustTick5));
    MH_CreateHook(convertFrom60, ConvertFrom60Hook, reinterpret_cast<void**>(&ConvertFrom60));
    MH_CreateHook(convertTo60, ConvertTo60Hook, reinterpret_cast<void**>(&ConvertTo60));
    MH_CreateHook(setTitleScreenPlaybackSpeed, SetTitleScreenPlaybackSpeedHook, reinterpret_cast<void**>(&SetTitleScreenPlaybackSpeed));
    MH_CreateHook(updateTitleScreenLayer, UpdateTitleScreenLayerHook, reinterpret_cast<void**>(&UpdateTitleScreenLayer));

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) 
    {
        spdlog::error("Failed to create or enable one or more hooks");
        MH_Uninitialize();
        return false;
    }

    spdlog::info("All hooks installed successfully");
    return true;
}

__int64 __fastcall MGS3FramerateUnlocker::SetTitleScreenPlaybackSpeedHook(__int64 a1, float a2)
{
    float scaled = a2 / instance->frameRateModifier;
    return instance->SetTitleScreenPlaybackSpeed(a1, scaled);
}

__int64 __fastcall MGS3FramerateUnlocker::UpdateTitleScreenLayerHook(__int64 a1)
{
    float delta = (static_cast<float>(instance->timeBase) / DEFAULT_TIMEBASE);

    float* pos_x = (float*)(a1 + 160);
    float* pos_y = (float*)(a1 + 164);

    float orig_x = *pos_x;
    float orig_y = *pos_y;

    *pos_x *= delta;
    *pos_y *= delta;

    __int64 result = instance->UpdateTitleScreenLayer(a1);

    *pos_x = orig_x;
    *pos_y = orig_y;

    return result;
}

bool MGS3FramerateUnlocker::InCutscene() const
{
    return gameVars.cutsceneFlag != nullptr && (*gameVars.cutsceneFlag > 0);
}

float MGS3FramerateUnlocker::CalculateMotionTimeBase(float value) const
{
    if (value <= 0.00f) 
    {
        value = static_cast<float>(*gameVars.timeBase);
    }

    value = value * (DEFAULT_TIMEBASE / static_cast<float>(*gameVars.timeBase));
    return value / frameRateModifier;
}

uint64_t __fastcall MGS3FramerateUnlocker::GetTimeBaseHook(struct _exception* a1)
{
    if (instance->InCutscene()) 
    {
        return DEFAULT_TIMEBASE;
    }

    return instance->timeBase;
}

__int64 __fastcall MGS3FramerateUnlocker::AdjustTickHook(struct _exception* a1)
{
    unsigned int tick = (unsigned int)a1;
    return (tick * 5 + instance->timeBase / 2) / instance->timeBase;
}

__int64 __fastcall MGS3FramerateUnlocker::AdjustTick2Hook(struct _exception* a1)
{
    unsigned int tick = (unsigned int)a1;
    return (tick * instance->timeBase) / 5;
}

float __fastcall MGS3FramerateUnlocker::AdjustTick3Hook(float tick)
{
    return tick * instance->timeBaseFloat / 5.0f;
}

float __fastcall MGS3FramerateUnlocker::AdjustTick5Hook(float tick)
{
    return tick * 5.0f / instance->timeBaseFloat;
}

__int64 __fastcall MGS3FramerateUnlocker::ConvertFrom60Hook(struct _exception* a1)
{
    unsigned int frames = (unsigned int)a1;
    return (int)((int64_t)frames * 5 / instance->timeBase);
}

__int64 __fastcall MGS3FramerateUnlocker::ConvertTo60Hook(struct _exception* a1)
{
    unsigned int frames = (unsigned int)a1;
    return (int)((int64_t)frames * instance->timeBase / 5);
}

void __fastcall MGS3FramerateUnlocker::UpdateMotionTimeBaseAHook(uint64_t a1, float a2, int a3)
{
    instance->UpdateMotionTimeBaseA(a1, instance->CalculateMotionTimeBase(a2), a3);
}

void __fastcall MGS3FramerateUnlocker::UpdateMotionTimeBaseBHook(uint64_t a1, float a2)
{
    instance->UpdateMotionTimeBaseB(a1, instance->CalculateMotionTimeBase(a2));
}

int64_t __fastcall MGS3FramerateUnlocker::GetTargetFpsHook(struct _exception* a1)
{
    return Config.targetFramerate;
}

int64_t __fastcall MGS3FramerateUnlocker::ThrowItemHook(uint64_t a1, int* a2, float* a3, int a4, uint16_t a5, int a6, int a7, int a8, struct _exception* a9, int a10)
{
    *(float*)&a6 = *(float*)&a6 / instance->frameRateModifier;
    return instance->ThrowItem(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
}

int16_t __fastcall MGS3FramerateUnlocker::UpdateAnimationBlendingHook(int16_t currentFrame, int16_t targetFrame)
{
    const float smoothingFactor = SMOOTHING_FACTOR * instance->frameRateModifier;

    int16_t diff = targetFrame - currentFrame;
    int16_t value = static_cast<int16_t>(static_cast<float>(diff) / smoothingFactor);

    if (*instance->gameVars.timeBase != instance->GetTimeBase(nullptr)) 
    {
        currentFrame = (value != 0) ? (currentFrame + value) : targetFrame;
    }

    return (value != 0) ? (currentFrame + value) : targetFrame;
}

double MGS3FramerateUnlocker::GetActorExecTimeHook()
{
    instance->execTime = instance->GetActorExecTime();

    if (instance->InCutscene())
    {
        *instance->gameVars.timeBase = DEFAULT_TIMEBASE;
        *instance->gameVars.actorWaitValue = 1.0 / DEFAULT_FPS;
    }
    else
    {
        *instance->gameVars.timeBase = instance->timeBase;
        *instance->gameVars.actorWaitValue = 1.0 / static_cast<double>(Config.targetFramerate);
    }

    return instance->execTime;
}

bool MGS3FramerateUnlocker::Initialize()
{
    spdlog::info("Initializing framerate unlocker...");

    if (Config.gameVersion < 0x1000200000000)
    {
        spdlog::error("Unsupported game version: {:#x}. Version 1.2 or higher is required.", Config.gameVersion);
        return false;
    }

    if (!InitializeOffsets())
    {
        spdlog::error("Failed to initialize offsets");
        return false;
    }

    if (!InstallHooks())
    {
        spdlog::error("Failed to install hooks");
        return false;
    }

    frameRateModifier = static_cast<float>(Config.targetFramerate) / DEFAULT_FPS;
    timeBase = std::round(DEFAULT_TIMEBASE / frameRateModifier - 0.1);
    timeBaseFloat = DEFAULT_TIMEBASE / frameRateModifier;

    // Apply initial values
    *gameVars.timeBase = timeBase;
    *gameVars.actorWaitValue = 1.0 / static_cast<double>(Config.targetFramerate);
    *gameVars.cameraSpeedModifierA = *gameVars.cameraSpeedModifierA / frameRateModifier;
    *gameVars.cameraSpeedModifierB = *gameVars.cameraSpeedModifierB / frameRateModifier;
    *gameVars.realTimeRate = *gameVars.realTimeRate / frameRateModifier;

    spdlog::info("Initialization complete");
    return true;
}

void MGS3_Initialize()
{
    spdlog::info("Starting framerate unlocker");

    MGS3FramerateUnlocker unlocker;

    if (!unlocker.Initialize())
    {
        spdlog::error("Failed to initialize the framerate unlocker");
        return;
    }

    Sleep(INFINITE);
}