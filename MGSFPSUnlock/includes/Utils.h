#pragma once

#include <shlwapi.h>
#include "config.h"

void GetGameType(HMODULE gameModule, GameType& result);
uint64_t GetGameVersion(HMODULE gameModule);