#pragma once

#include <Windows.h>
#include <cstdint>

namespace Memory 
{
    void DetourFunction(uint64_t target, LPVOID detour, LPVOID* ppOriginal);
    uintptr_t PatternScanBasic(uintptr_t beg, uintptr_t end, uint8_t* str, uintptr_t len);
    uint8_t* PatternScan(void* module, const char* signature);
};