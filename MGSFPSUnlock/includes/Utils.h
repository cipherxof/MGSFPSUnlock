#pragma once

#include <shlwapi.h>
#include "config.h"

void GetGameType(HMODULE gameModule, GameType& result);
uint64_t GetGameVersion(HMODULE gameModule);
void LogAddress(std::string name, uintptr_t addr);
uintptr_t GetRelativeOffset(uint8_t* addr);