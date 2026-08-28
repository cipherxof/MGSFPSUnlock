#include <windows.h>
#include <cstdint>
#include <iostream>
#include <stdio.h>
#include <vector>
#include "Memory.h"
#include "MinHook.h"

namespace
{
    std::vector<int> PatternToBytes(const char* pattern)
    {
        std::vector<int> bytes;
        auto start = const_cast<char*>(pattern);
        auto end = start + strlen(pattern);

        for (auto current = start; current < end; ++current)
        {
            if (*current == '?')
            {
                ++current;
                if (current < end && *current == '?')
                    ++current;
                bytes.push_back(-1);
            }
            else
            {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }

        return bytes;
    }
}

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
uint8_t* Memory::PatternScan(void* module, const char* signature, int skip, bool end)
{
    auto dosHeader = (PIMAGE_DOS_HEADER)module;
    auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);

    auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    return PatternScanRange(module, sizeOfImage, signature, skip, end);
}

uint8_t* Memory::PatternScanRange(void* begin, uintptr_t size, const char* signature, int skip, bool end)
{
    auto patternBytes = PatternToBytes(signature);
    auto scanBytes = reinterpret_cast<std::uint8_t*>(begin);

    auto s = patternBytes.size();
    auto d = patternBytes.data();
    int foundCount = 0;

    if (size < s)
        return nullptr;

    for (uintptr_t i = 0; i <= size - s; ++i) {
        bool found = true;
        for (uintptr_t j = 0; j < s; ++j) {
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
