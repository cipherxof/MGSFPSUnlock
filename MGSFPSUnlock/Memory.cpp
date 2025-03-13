#include <windows.h>
#include <cstdint>
#include <iostream>
#include <stdio.h>
#include <vector>
#include "Memory.h"
#include "MinHook.h"

void Memory::DetourFunction(uint64_t target, LPVOID detour, LPVOID* ppOriginal)
{
    int error = 0;
    if (error = MH_CreateHook((LPVOID)target, detour, ppOriginal) != 0)
    {
        return;
    }
    MH_EnableHook((LPVOID)target);
}

uintptr_t Memory::PatternScanBasic(uintptr_t beg, uintptr_t end, uint8_t* str, uintptr_t len)
{
    for (uintptr_t ptr = beg; ptr < end - len; ++ptr)
    {
        if (0 == memcmp((const void*)ptr, str, len))
            return ptr;
        ptr++;
    }

    return 0;
}

// CSGOSimple's pattern scan
// https://github.com/OneshotGH/CSGOSimple-master/blob/master/CSGOSimple/helpers/utils.cpp
uint8_t* Memory::PatternScan(void* module, const char* signature, int skip = 0, bool end = false)
{
    static auto pattern_to_byte = [](const char* pattern) {
        auto bytes = std::vector<int>{};
        auto start = const_cast<char*>(pattern);
        auto end = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?')
                    ++current;
                bytes.push_back(-1);
            }
            else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
    };

    auto dosHeader = (PIMAGE_DOS_HEADER)module;
    auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);

    auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    auto patternBytes = pattern_to_byte(signature);
    auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

    auto s = patternBytes.size();
    auto d = patternBytes.data();
    int foundCount = 0;

    for (auto i = 0ul; i < sizeOfImage - s; ++i) {
        bool found = true;
        for (auto j = 0ul; j < s; ++j) {
            if (scanBytes[i + j] != d[j] && d[j] != -1) {
                found = false;
                break;
            }
        }
        if (found) {
            foundCount++;
            if (foundCount > skip) {
                return end ? &scanBytes[i] + s : &scanBytes[i];
            }
        }
    }
    return nullptr;
}