#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
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
    typedef __int64 __fastcall GetTimeBaseDelegate(struct _exception* a1);

    // Game data structure
    struct GameVariables 
    {
        double* actorWaitValue;
        int* cutsceneFlag;
        int* demoCutsceneFlag;
    };

    // Original function pointers
    GetTimeBaseDelegate* GetTimeBase;

    // Game data
    GameVariables gameVars;
    float frameRateModifier;
    int timeBase;

    // Private methods
    bool InitializeOffsets();
    bool InstallHooks();
    bool InCutscene() const;
    bool InDemoCutscene() const;


    // Hook methods
    static MGS2FramerateUnlocker* instance;

    static __int64 __fastcall GetTimeBaseHook(struct _exception* a1);
};

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

__int64 __fastcall MGS2FramerateUnlocker::GetTimeBaseHook(struct _exception* a1)
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
        instance->gameVars.cutsceneFlag = reinterpret_cast<int*>(GameBase + 0xAA9F94);
        instance->gameVars.demoCutsceneFlag = reinterpret_cast<int*>(GameBase + 0x16CB5D8);
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

    if (!getTimeBaseOffset)
    {
        spdlog::error("Failed to find one or more function patterns");
        return false;
    }

    spdlog::debug("getTimeBaseOffset = {:#x}", reinterpret_cast<uintptr_t>(getTimeBaseOffset) - reinterpret_cast<uintptr_t>(GameModule));

    MH_CreateHook(getTimeBaseOffset, GetTimeBaseHook, reinterpret_cast<void**>(&GetTimeBase));

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

    if (Config.targetFramerate > DEFAULT_FPS) { //A LOT more work is needed before we support unlocked framerate.
        Config.targetFramerate = DEFAULT_FPS;
        spdlog::warn("MGS2 currently only supports framerate related bugfixes and not fully unlocked FPS. Setting target to {}.", DEFAULT_FPS);
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