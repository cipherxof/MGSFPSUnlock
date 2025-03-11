#pragma once

#include "ini.h"

enum class GameType
{
    Unknown,
    MGS2,
    MGS3
};

struct GameConfig {
    GameType gameType;
    uint64_t gameVersion;
    int targetFramerate;
};

extern GameConfig Config;
extern HMODULE GameModule;
extern uintptr_t GameBase;
extern mINI::INIStructure ConfigValues;