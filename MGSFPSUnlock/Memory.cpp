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

bool Memory::GetModuleSection(HMODULE module, const char* name, ModuleSection& result)
{
    result = {};
    if (!module || !name)
        return false;

    const size_t nameLength = strlen(name);
    if (nameLength == 0 || nameLength > IMAGE_SIZEOF_SHORT_NAME)
        return false;

    const auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        return false;

    const auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
        reinterpret_cast<uint8_t*>(module) + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
        return false;

    const auto expectedName = reinterpret_cast<const uint8_t*>(name);
    auto section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD index = 0; index < ntHeaders->FileHeader.NumberOfSections; ++index, ++section)
    {
        if (memcmp(section->Name, expectedName, nameLength) != 0 ||
            (nameLength < IMAGE_SIZEOF_SHORT_NAME && section->Name[nameLength] != '\0'))
            continue;

        result.begin = reinterpret_cast<uint8_t*>(module) + section->VirtualAddress;
        result.size = static_cast<uintptr_t>(section->Misc.VirtualSize);
        return result.size != 0;
    }

    return false;
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
