#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "ini.h"
#include "config.h"
#include "spdlog/spdlog.h"

class MGS2FramerateUnlocker
{
public:
    MGS2FramerateUnlocker();
    ~MGS2FramerateUnlocker();

    bool Initialize();
    void RunUpdateLoop();

private:
    // Constants
    static constexpr int DEFAULT_TIMEBASE = 5;
    static constexpr float DEFAULT_FPS = 60.0f;

    // Function delegates
    typedef __int64 __fastcall ActDashFireDelegate(__int64 a1);
    typedef __int64 __fastcall CreateDebrisTexDelegate(__int64 a1, float* a2, float* a3, unsigned int a4, int a5, int a6, float a7);
    typedef __int64 __fastcall GetTimeBaseDelegate(int a1);
    typedef __int64 __fastcall sub_144A0Delegate(int a1);
    typedef __int64 __fastcall GetTargetFpsDelegate(int a1);
    typedef __int64 __fastcall sub_142C0Delegate(int a1);
    typedef __int64 __fastcall sub_15CDF0Delegate(__int64 a1);
    typedef __int64 __fastcall sub_4CACF0Delegate(__int64 a1, int* a2, int a3, int a4, int a5, __int16 a6, float a7, int a8, int a9);
    typedef __int64 __fastcall sub_6ED70Delegate(unsigned __int8* a1, __int64 a2);
    typedef __int64 __fastcall sub_53EFF0Delegate(__int64 a1);

    // Game data structure
    struct GameVariables 
    {
        int* timeBase;
        double* actorWaitValue;
        int* cutsceneFlag;
        int* demoCutsceneFlag;

        float* frameScaleFactorA;
        float* frameScaleFactorB; // sadly reused by non-frame related funcs too
    };

    // Original function pointers
    GetTimeBaseDelegate* GetTimeBase;
    ActDashFireDelegate* ActDashFire;
    CreateDebrisTexDelegate* CreateDebrisTex;
    sub_144A0Delegate* sub_144A0;
    GetTargetFpsDelegate* GetTargetFps;
    sub_142C0Delegate* sub_142C0;
    sub_15CDF0Delegate* sub_15CDF0;
    sub_4CACF0Delegate* sub_4CACF0;
    sub_6ED70Delegate* sub_6ED70;
    sub_53EFF0Delegate* sub_53EFF0;

    // Game data
    GameVariables gameVars;
    float frameRateModifier;
    int timeBase;

    // Private methods
    bool InitializeOffsets();
    bool InstallHooks();
    bool InCutscene() const;
    bool InDemoCutscene() const;

    // Private members
    double dashLastUpdate = 0;

    // Hook methods
    static MGS2FramerateUnlocker* instance;

    static __int64 __fastcall ActDashFireHook(__int64 a1);
    static __int64 __fastcall CreateDebrisTexHook(__int64 a1, float* a2, float* a3, unsigned int a4, int a5, int a6, float a7);
    static __int64 __fastcall GetTimeBaseHook(int a1);
    static __int64 __fastcall sub_144A0Hook(int a1);
    static __int64 __fastcall GetTargetFpsHook(int a1);
    static __int64 __fastcall sub_142C0Hook(int a1);
    static __int64 __fastcall sub_15CDF0Hook(__int64 a1);
    static __int64 __fastcall sub_4CACF0Hook(__int64 a1, int* a2, int a3, int a4, int a5, __int16 a6, float a7, int a8, int a9);
    static __int64 __fastcall sub_6ED70Hook(unsigned __int8* a1, __int64 a2);
    static __int64 __fastcall sub_53EFF0Hook(__int64 a1);
};

__int64 __fastcall MGS2FramerateUnlocker::sub_144A0Hook(int a1)
{
    float scale = DEFAULT_FPS / Config.targetFramerate;

    *instance->gameVars.frameScaleFactorA = -2.7222223f * (scale * scale);

    return 0;
}

__int64 __fastcall MGS2FramerateUnlocker::GetTargetFpsHook(int a1)
{
    return Config.targetFramerate;
}

__int64 __fastcall MGS2FramerateUnlocker::sub_142C0Hook(int a1)
{
    return (a1 * Config.targetFramerate + 30) / 60;
}

__int64 __fastcall MGS2FramerateUnlocker::sub_15CDF0Hook(__int64 a1)
{
    auto result = instance->sub_15CDF0(a1);

    auto v4 = *(__int64*)(a1 + 8);
    auto v6 = *(float*)(v4 + 20) + 16.0f;
    auto scale = DEFAULT_FPS / Config.targetFramerate;
    *(float*)(v4 + 20) = v6 * scale;

    return result;
}

__int64 __fastcall MGS2FramerateUnlocker::sub_4CACF0Hook(__int64 a1, int* a2, int a3, int a4, int a5, __int16 a6, float a7, int a8, int a9)
{
    a7 = a7 * (DEFAULT_FPS / Config.targetFramerate);
    return instance->sub_4CACF0(a1, a2, a3, a4, a5, a6, a7, a8, a9);
}

__int64 __fastcall MGS2FramerateUnlocker::sub_6ED70Hook(unsigned __int8* a1, __int64 a2)
{
    auto v6 = instance->sub_6ED70(a1, a2);

    *(float*)(v6 + 164) = DEFAULT_FPS / Config.targetFramerate;

    return v6;
}
 
__int64 __fastcall MGS2FramerateUnlocker::sub_53EFF0Hook(__int64 a1) // fps cam speed?
{
    *instance->gameVars.frameScaleFactorB = DEFAULT_FPS / Config.targetFramerate;

    auto result = instance->sub_53EFF0(a1);

    *instance->gameVars.frameScaleFactorB = 1.2f;

    return result;
}

MGS2FramerateUnlocker* MGS2FramerateUnlocker::instance = nullptr;

MGS2FramerateUnlocker::MGS2FramerateUnlocker()
    : frameRateModifier(1.0f), timeBase(DEFAULT_TIMEBASE)
{
    instance = this;
    memset(&gameVars, 0, sizeof(GameVariables));
}

MGS2FramerateUnlocker::~MGS2FramerateUnlocker()
{
    instance = nullptr;
}

bool MGS2FramerateUnlocker::InCutscene() const
{
    return gameVars.cutsceneFlag == NULL ? false : (*gameVars.cutsceneFlag == 1);
}

bool MGS2FramerateUnlocker::InDemoCutscene() const
{
    return gameVars.demoCutsceneFlag == NULL ? false : (*gameVars.demoCutsceneFlag == 1);
}

__int64 __fastcall MGS2FramerateUnlocker::ActDashFireHook(__int64 a1)
{   
    if (!instance->InDemoCutscene()) // only slow down during demo cutscenes. boss fight runs at normal gamespeed.
        return instance->ActDashFire(a1);

    instance->dashLastUpdate += *instance->gameVars.actorWaitValue;

    if (instance->dashLastUpdate < 1.0 / 30.0)
        return 0;

    instance->dashLastUpdate = 0;

    return instance->ActDashFire(a1);
}

__int64 __fastcall MGS2FramerateUnlocker::CreateDebrisTexHook(__int64 a1, float* a2, float* a3, unsigned int a4, int a5, int a6, float a7)
{
    auto result = instance->CreateDebrisTex(a1, a2, a3, a4, a5, a6, a7);

    *(int*)(a1 + 100) = 15 * (instance->GetTimeBase(0) * ((Config.targetFramerate / 60) + 1)); // effect duration

    return result;
}

__int64 __fastcall MGS2FramerateUnlocker::GetTimeBaseHook(int a1)
{
    return instance->timeBase;
}

bool MGS2FramerateUnlocker::InitializeOffsets()
{
    spdlog::info("Initializing offsets...");

    instance->gameVars.actorWaitValue = (double*)(Memory::PatternScan(GameModule, "42 50 5F 4D 45 4D 4A 50 45 47") + 0x18);

    switch (Config.gameVersion) 
    {
    case 0x1000200000000:
        instance->gameVars.cutsceneFlag = reinterpret_cast<int*>(GameBase + 0xAADA64);
        instance->gameVars.demoCutsceneFlag = reinterpret_cast<int*>(GameBase + 0x16CE4B8);
        break;
    case 0x1000300000000:
        instance->gameVars.cutsceneFlag = reinterpret_cast<int*>(GameBase + 0xAADA64);
        instance->gameVars.demoCutsceneFlag = reinterpret_cast<int*>(GameBase + 0x16CE4B8);
        break;
    case 0x1000400000000:
        instance->gameVars.cutsceneFlag = reinterpret_cast<int*>(GameBase + 0xAB4C74);
        instance->gameVars.demoCutsceneFlag = reinterpret_cast<int*>(GameBase + 0x16D5638);
        break;
    case 0x1000400010000:
        instance->gameVars.cutsceneFlag = reinterpret_cast<int*>(GameBase + 0xAB4C74);
        instance->gameVars.demoCutsceneFlag = reinterpret_cast<int*>(GameBase + 0x16D5638);
        break;
    case 0x1000500010000:
        instance->gameVars.cutsceneFlag = reinterpret_cast<int*>(GameBase + 0xA8CD74);
        instance->gameVars.demoCutsceneFlag = reinterpret_cast<int*>(GameBase + 0x16AD878);
        break;
    case 0x2000000000000:
        instance->gameVars.cutsceneFlag = reinterpret_cast<int*>(GameBase + 0xAA8F84);
        instance->gameVars.demoCutsceneFlag = reinterpret_cast<int*>(GameBase + 0x16CA5D8);
        break;
    case 0x2000000010000:
        instance->gameVars.timeBase = reinterpret_cast<int*>(GameBase + 0xAA9F94);
        instance->gameVars.cutsceneFlag = reinterpret_cast<int*>(GameBase + 0xAA9F94);
        instance->gameVars.demoCutsceneFlag = reinterpret_cast<int*>(GameBase + 0x16CB5D8);

        instance->gameVars.frameScaleFactorA = reinterpret_cast<float*>(GameBase + 0x16ED790);
        instance->gameVars.frameScaleFactorB = reinterpret_cast<float*>(GameBase + 0x725314);
        break;
    default:
        spdlog::error("Unsupported game version: {:#x}", Config.gameVersion);
        return false;
    }

    if (!instance->gameVars.actorWaitValue ||
        !instance->gameVars.cutsceneFlag ||
        !instance->gameVars.demoCutsceneFlag)
    {
        spdlog::error("Failed to initialize one or more critical offsets");
        return false;
    }

    spdlog::debug("actorWaitValue = {:#x}", reinterpret_cast<uintptr_t>(gameVars.actorWaitValue) - GameBase);
    spdlog::debug("cutsceneFlag = {:#x}", reinterpret_cast<uintptr_t>(gameVars.cutsceneFlag) - GameBase);
    spdlog::debug("demoCutsceneFlag = {:#x}", reinterpret_cast<uintptr_t>(gameVars.demoCutsceneFlag) - GameBase);

    DWORD oldProtect;
    VirtualProtect(instance->gameVars.actorWaitValue, 8, PAGE_READWRITE, &oldProtect);
    VirtualProtect(instance->gameVars.frameScaleFactorB, 4, PAGE_READWRITE, &oldProtect);
    VirtualProtect((LPVOID)(GameBase + 0x53F3F3), 2, PAGE_EXECUTE_READWRITE, &oldProtect);
    *(uint8_t*)(GameBase + 0x53F3F3) = 0x90;
    *(uint8_t*)(GameBase + 0x53F3F4) = 0x90;
    spdlog::info("InitializeOffsets() finished.");
    return true;
}

bool MGS2FramerateUnlocker::InstallHooks()
{
    spdlog::info("Installing hooks");

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK)
    {
        spdlog::error("Failed to initialize MinHook, status: {}", (int)status);
        return false;
    }

    uint8_t* getTimeBaseOffset = Memory::PatternScan(GameModule, "48 83 EC 28 E8 ?? ?? ?? ?? 33 C9 83 F8 01 0F 94");
    uint8_t* actDashFireOffset = Memory::PatternScan(GameModule, "?? ?? ?? ?? ?? 49 8D AB 68 FE FF FF 48 81 EC 88");
    uint8_t* createDebrisTexOffset = Memory::PatternScan(GameModule, "40 55 53 56 57 41 54 41 56 41 57 48 8D AC 24 00");

    if (!actDashFireOffset || !createDebrisTexOffset || !getTimeBaseOffset)
    {
        spdlog::error("Failed to find one or more function patterns");
        return false;
    }

    spdlog::debug("getTimeBaseOffset = {:#x}", reinterpret_cast<uintptr_t>(getTimeBaseOffset) - reinterpret_cast<uintptr_t>(GameModule));
    spdlog::debug("actDashFireOffset = {:#x}", reinterpret_cast<uintptr_t>(actDashFireOffset) - reinterpret_cast<uintptr_t>(GameModule));
    spdlog::debug("createDebrisTexOffset = {:#x}", reinterpret_cast<uintptr_t>(createDebrisTexOffset) - reinterpret_cast<uintptr_t>(GameModule));

    MH_CreateHook(getTimeBaseOffset, GetTimeBaseHook, reinterpret_cast<void**>(&GetTimeBase));
    MH_CreateHook(createDebrisTexOffset, CreateDebrisTexHook, reinterpret_cast<void**>(&CreateDebrisTex));
    MH_CreateHook(actDashFireOffset, ActDashFireHook, reinterpret_cast<void**>(&ActDashFire));
    //MH_CreateHook((LPVOID)((uintptr_t)GameModule + 0xC780), Is50HzHook, reinterpret_cast<void**>(&Is50Hz));
    MH_CreateHook((LPVOID)((uintptr_t)GameModule + 0x144A0), sub_144A0Hook, reinterpret_cast<void**>(&sub_144A0));
    MH_CreateHook((LPVOID)((uintptr_t)GameModule + 0x143F0), GetTargetFpsHook, reinterpret_cast<void**>(&GetTargetFps));
    MH_CreateHook((LPVOID)((uintptr_t)GameModule + 0x142C0), sub_142C0Hook, reinterpret_cast<void**>(&sub_142C0));
    MH_CreateHook((LPVOID)((uintptr_t)GameModule + 0x15CDF0), sub_15CDF0Hook, reinterpret_cast<void**>(&sub_15CDF0));
    MH_CreateHook((LPVOID)((uintptr_t)GameModule + 0x4CACF0), sub_4CACF0Hook, reinterpret_cast<void**>(&sub_4CACF0));
    MH_CreateHook((LPVOID)((uintptr_t)GameModule + 0x6ED70), sub_6ED70Hook, reinterpret_cast<void**>(&sub_6ED70));
    MH_CreateHook((LPVOID)((uintptr_t)GameModule + 0x53EFF0), sub_53EFF0Hook, reinterpret_cast<void**>(&sub_53EFF0));


    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
    {
        spdlog::error("Failed to create or enable one or more hooks");
        MH_Uninitialize();
        return false;
    }

    spdlog::info("All hooks installed successfully");
    return true;
}

void MGS2FramerateUnlocker::RunUpdateLoop()
{
    while (true)
    {
        if (gameVars.actorWaitValue != nullptr)
        {
            if (InCutscene())
            {
                timeBase = DEFAULT_TIMEBASE;
                *gameVars.actorWaitValue = 1.0 / DEFAULT_FPS;
            }
            else
            {
                auto mod = (float)Config.targetFramerate / DEFAULT_FPS;
                timeBase = std::round(DEFAULT_TIMEBASE / mod - 0.1);
                *gameVars.actorWaitValue = 1.0 / (double)Config.targetFramerate;
            }
        }
        Sleep(1);
    }
}

bool MGS2FramerateUnlocker::Initialize()
{
    spdlog::info("Initializing framerate unlocker...");

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
    *gameVars.actorWaitValue = 1.0 / static_cast<double>(Config.targetFramerate);

    spdlog::info("Initialization complete. Target framerate: {}", Config.targetFramerate);
    return true;
}

void MGS2_Initialize()
{
    spdlog::info("Starting framerate unlocker");

    MGS2FramerateUnlocker unlocker;

    if (!unlocker.Initialize())
    {
        spdlog::error("Failed to initialize the framerate unlocker");
        return;
    }

    unlocker.RunUpdateLoop(); // should maybe hook actorExec instead
}