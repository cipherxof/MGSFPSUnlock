#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "ini.h"
#include "config.h"
#include "spdlog/spdlog.h"

class MGS3FramerateUnlocker 
{
public:
    MGS3FramerateUnlocker();
    ~MGS3FramerateUnlocker();

    bool Initialize();
    void RunUpdateLoop();

private:
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

    // Game data
    GameVariables gameVars;
    float frameRateModifier;
    int timeBase;

    // Private methods
    bool InitializeOffsets();
    bool InstallHooks();
    bool InCutscene() const;
    float CalculateMotionTimeBase(float value) const;

    // Hook methods
    static MGS3FramerateUnlocker* instance;

    static uint64_t __fastcall GetTimeBaseHook(struct _exception* a1);
    static void __fastcall UpdateMotionTimeBaseAHook(uint64_t a1, float a2, int a3);
    static void __fastcall UpdateMotionTimeBaseBHook(uint64_t a1, float a2);
    static int64_t __fastcall GetTargetFpsHook(struct _exception* a1);
    static int64_t __fastcall ThrowItemHook(uint64_t a1, int* a2, float* a3, int a4, uint16_t a5, int a6, int a7, int a8, struct _exception* a9, int a10);
    static int16_t __fastcall UpdateAnimationBlendingHook(int16_t currentFrame, int16_t targetFrame);
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

uintptr_t GetRelativeOffset(uint8_t* addr)
{
    return reinterpret_cast<uintptr_t>(addr) + 4 + *reinterpret_cast<int32_t*>(addr);
}

bool MGS3FramerateUnlocker::InitializeOffsets()
{
    spdlog::info("Initializing offsets...");
    
    uint8_t* addr = nullptr;
    memset(&gameVars, 0, sizeof(GameVariables));

    addr = Memory::PatternScan(GameModule, "33 C9 83 F8 01 ?? ?? C1 83 C1 05");
    if (!addr) spdlog::error("Could not find GV_TimeBase");
    else gameVars.timeBase = GetRelativeOffset(addr + 13);

    addr = Memory::PatternScan(GameModule, "83 3D ?? ?? ?? ?? 00 ?? ?? F2 0F 10 0D");
    if (!addr) spdlog::error("Could not find ActorWaitValue");
    else gameVars.actorWaitValue = GetRelativeOffset(addr + 13);

    addr = Memory::PatternScan(GameModule, "89 05 ?? ?? ?? ?? B1 65 E8 ?? ?? ?? FF 8B 0D");
    if (!addr) spdlog::error("Could not find CutsceneFlag");
    else gameVars.cutsceneFlag = GetRelativeOffset(addr + 2);

    addr = Memory::PatternScan(GameModule, "0F 28 F0 E8 ?? ?? ?? ?? F3 0F 10 0D ?? ?? ?? ??");
    if (!addr) spdlog::error("Could not find CameraSpeedModifierA");
    else gameVars.cameraSpeedModifierA = GetRelativeOffset(addr + 12);

    addr = Memory::PatternScan(GameModule, "0F 28 ?? F3 0F 10 87 90 00 00 00 F3 0F 10 15");
    if (!addr) spdlog::error("Could not find CameraSpeedModifierB");
    else gameVars.cameraSpeedModifierB = GetRelativeOffset(addr + 15);

    addr = Memory::PatternScan(GameModule, "0F 6E C0 0F 5B C0 F3 0F 59 05 ?? ?? ?? ?? F3 48 0F 2C C0", 1);
    if (!addr) spdlog::error("Could not find RealTimeRate");
    else gameVars.realTimeRate = GetRelativeOffset(addr + 10);

    // set offsets manually here (useful for development)
    switch (Config.gameVersion) 
    {
    case 0x2000000010000:
        break;
    default:
        break;
    }

    spdlog::debug("timeBase = {:#x}", reinterpret_cast<uintptr_t>(gameVars.timeBase) - GameBase);
    spdlog::debug("actorWaitValue = {:#x}", reinterpret_cast<uintptr_t>(gameVars.actorWaitValue) - GameBase);
    spdlog::debug("cutsceneFlag = {:#x}", reinterpret_cast<uintptr_t>(gameVars.cutsceneFlag) - GameBase);
    spdlog::debug("cameraSpeedModifierA = {:#x}", reinterpret_cast<uintptr_t>(gameVars.cameraSpeedModifierA) - GameBase);
    spdlog::debug("cameraSpeedModifierB = {:#x}", reinterpret_cast<uintptr_t>(gameVars.cameraSpeedModifierB) - GameBase);
    spdlog::debug("realTimeRate = {:#x}", reinterpret_cast<uintptr_t>(gameVars.realTimeRate) - GameBase);

    if (!gameVars.timeBase || !gameVars.actorWaitValue || !gameVars.cameraSpeedModifierA ||
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

    uint8_t* getTimeBaseAddr = Memory::PatternScan(GameModule, "48 83 EC 28 E8 ?? ?? FD FF 33 C9 83 F8 01 0F 94");
    uint8_t* updateMotionAAddr = Memory::PatternScan(GameModule, "48 85 C9 74 31 0F 57 C0 0F 2F C1 76 0B 66 0F 6E");
    uint8_t* updateMotionBAddr = Memory::PatternScan(GameModule, "48 85 C9 74 3F 0F 57 C0 0F 2F C1 76 0B 66 0F 6E");
    uint8_t* getTargetFpsAddr = Memory::PatternScan(GameModule, "48 83 EC 28 E8 ?? ?? FD FF 83 F8 01 B9 3C 00 00");
    uint8_t* throwItemAddr = Memory::PatternScan(GameModule, "40 55 56 57 41 56 41 57 48 8D 6C 24 E0 48 81 EC");
    uint8_t* animBlendAddr = Memory::PatternScan(GameModule, "48 89 5C 24 08 57 48 83 EC 30 0F 29 74 24 20 0F");

    if (!getTimeBaseAddr || !updateMotionAAddr || !updateMotionBAddr ||
        !getTargetFpsAddr || !throwItemAddr || !animBlendAddr) 
    {
        spdlog::error("Failed to find one or more function patterns");
        return false;
    }

    spdlog::debug("getTimeBase = {:#x}", reinterpret_cast<uintptr_t>(getTimeBaseAddr) - reinterpret_cast<uintptr_t>(GameModule));
    spdlog::debug("updateMotionA = {:#x}", reinterpret_cast<uintptr_t>(updateMotionAAddr) - reinterpret_cast<uintptr_t>(GameModule));
    spdlog::debug("updateMotionB = {:#x}", reinterpret_cast<uintptr_t>(updateMotionBAddr) - reinterpret_cast<uintptr_t>(GameModule));
    spdlog::debug("getTargetFps = {:#x}", reinterpret_cast<uintptr_t>(getTargetFpsAddr) - reinterpret_cast<uintptr_t>(GameModule));
    spdlog::debug("throwItem = {:#x}", reinterpret_cast<uintptr_t>(throwItemAddr) - reinterpret_cast<uintptr_t>(GameModule));
    spdlog::debug("animBlend = {:#x}", reinterpret_cast<uintptr_t>(animBlendAddr) - reinterpret_cast<uintptr_t>(GameModule));

    MH_CreateHook(getTimeBaseAddr, GetTimeBaseHook, reinterpret_cast<void**>(&GetTimeBase));
    MH_CreateHook(updateMotionAAddr, UpdateMotionTimeBaseAHook, reinterpret_cast<void**>(&UpdateMotionTimeBaseA));
    MH_CreateHook(updateMotionBAddr, UpdateMotionTimeBaseBHook, reinterpret_cast<void**>(&UpdateMotionTimeBaseB));
    MH_CreateHook(getTargetFpsAddr, GetTargetFpsHook, reinterpret_cast<void**>(&GetTargetFps));
    MH_CreateHook(throwItemAddr, ThrowItemHook, reinterpret_cast<void**>(&ThrowItem));
    MH_CreateHook(animBlendAddr, UpdateAnimationBlendingHook, reinterpret_cast<void**>(&UpdateAnimationBlending));

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) 
    {
        spdlog::error("Failed to create or enable one or more hooks");
        MH_Uninitialize();
        return false;
    }

    spdlog::info("All hooks installed successfully");
    return true;
}

bool MGS3FramerateUnlocker::InCutscene() const
{
    return gameVars.cutsceneFlag != nullptr && (*gameVars.cutsceneFlag == 0);
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

void MGS3FramerateUnlocker::RunUpdateLoop()
{
    while (true)
    {
        if (gameVars.actorWaitValue != nullptr)
        {
            if (InCutscene())
            {
                *gameVars.timeBase = DEFAULT_TIMEBASE;
                *gameVars.actorWaitValue = 1.0 / DEFAULT_FPS;
            }
            else
            {
                *gameVars.timeBase = timeBase;
                *gameVars.actorWaitValue = 1.0 / static_cast<double>(Config.targetFramerate);
            }
        }
        Sleep(1);
    }
}

bool MGS3FramerateUnlocker::Initialize()
{
    spdlog::info("Initializing framerate unlocker...");

    if (Config.gameVersion < 0x1000200000000)
    {
        spdlog::error("Unsupported game version: {:#x}. Version 1.2 or higher is required.", Config.gameVersion);
        return;
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

    unlocker.RunUpdateLoop(); // should maybe hook actorExec instead
}