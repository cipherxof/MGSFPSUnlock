#pragma once

#include <windows.h>
#include <cstdint>

namespace Memory 
{
    struct ModuleSection
    {
        uint8_t* begin = nullptr;
        uintptr_t size = 0;
    };

    void DetourFunction(uint64_t target, LPVOID detour, LPVOID* ppOriginal);
    bool GetModuleSection(HMODULE module, const char* name, ModuleSection& result);
    uintptr_t PatternScanBasic(uintptr_t beg, uintptr_t end, uint8_t* str, uintptr_t len);
    uint8_t* PatternScan(void* module, const char* signature, int skip = 0, bool end = false);
    uint8_t* PatternScanRange(void* begin, uintptr_t size, const char* signature, int skip = 0, bool end = false);
};
