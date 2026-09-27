#include <windows.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdio.h>
#include <vector>
#include "Memory.h"
#include "MinHook.h"

namespace
{
    struct PatternByte
    {
        uint8_t mask = 0;
        uint8_t value = 0;
    };

    bool ParseNibble(char c, uint8_t& value, bool& wildcard)
    {
        if (c == '?')
        {
            wildcard = true;
            value = 0;
            return true;
        }

        wildcard = false;
        if (c >= '0' && c <= '9')
            value = static_cast<uint8_t>(c - '0');
        else if (c >= 'a' && c <= 'f')
            value = static_cast<uint8_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            value = static_cast<uint8_t>(c - 'A' + 10);
        else
            return false;

        return true;
    }

    bool ParsePattern(const char* pattern, std::vector<PatternByte>& result)
    {
        result.clear();
        if (!pattern)
            return false;

        for (const char* current = pattern; *current != '\0';)
        {
            if (*current == ' ' || *current == '\t')
            {
                ++current;
                continue;
            }

            const size_t available = (current[1] == '\0' || current[1] == ' ' || current[1] == '\t') ? 1 : 2;

            uint8_t high = 0;
            uint8_t low = 0;
            bool highWildcard = false;
            bool lowWildcard = false;

            if (available == 1)
            {
                if (*current != '?')
                    return false;
                highWildcard = lowWildcard = true;
                ++current;
            }
            else
            {
                if (!ParseNibble(current[0], high, highWildcard) || !ParseNibble(current[1], low, lowWildcard))
                    return false;
                current += 2;
            }

            PatternByte entry;
            if (!highWildcard)
            {
                entry.mask |= 0xF0;
                entry.value |= static_cast<uint8_t>(high << 4);
            }
            if (!lowWildcard)
            {
                entry.mask |= 0x0F;
                entry.value |= low;
            }

            result.push_back(entry);
        }

        return !result.empty();
    }

    bool MatchesAt(const uint8_t* candidate, const std::vector<PatternByte>& pattern)
    {
        for (size_t index = 0; index < pattern.size(); ++index)
        {
            if ((candidate[index] & pattern[index].mask) != pattern[index].value)
                return false;
        }

        return true;
    }

    uint8_t* LookupUnwindEntry(HMODULE module, const void* address)
    {
        const auto base = reinterpret_cast<uint8_t*>(module);
        const auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
            return nullptr;

        const auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
            return nullptr;

        const IMAGE_DATA_DIRECTORY& directory =
            ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if (directory.VirtualAddress == 0 || directory.Size < sizeof(RUNTIME_FUNCTION))
            return nullptr;

        const auto functions = reinterpret_cast<PRUNTIME_FUNCTION>(base + directory.VirtualAddress);
        const size_t count = directory.Size / sizeof(RUNTIME_FUNCTION);
        const auto target = static_cast<DWORD>(reinterpret_cast<const uint8_t*>(address) - base);

        size_t low = 0;
        size_t high = count;
        while (low < high)
        {
            const size_t middle = low + (high - low) / 2;
            if (functions[middle].BeginAddress <= target)
                low = middle + 1;
            else
                high = middle;
        }

        if (low == 0)
            return nullptr;

        const RUNTIME_FUNCTION& entry = functions[low - 1];
        if (target < entry.BeginAddress || target >= entry.EndAddress)
            return nullptr;

        return base + entry.BeginAddress;
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

bool Memory::PatchCode(void* address, const void* bytes, size_t size)
{
    DWORD oldProtect = 0;
    if (!address || !bytes || !VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    memcpy(address, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), address, size);
    VirtualProtect(address, size, oldProtect, &oldProtect);
    return true;
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
    std::vector<PatternByte> pattern;
    if (!ParsePattern(signature, pattern) || size < pattern.size())
        return nullptr;

    const auto scanBytes = reinterpret_cast<uint8_t*>(begin);
    int foundCount = 0;

    for (uintptr_t i = 0; i <= size - pattern.size(); ++i)
    {
        if (!MatchesAt(scanBytes + i, pattern))
            continue;

        if (foundCount++ >= skip)
            return end ? scanBytes + i + pattern.size() : scanBytes + i;
    }

    return nullptr;
}

uint8_t* Memory::GetFunctionStart(HMODULE module, void* address)
{
    if (!module || !address)
        return nullptr;

    DWORD64 imageBase = 0;
    if (PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(address), &imageBase, nullptr))
        return reinterpret_cast<uint8_t*>(imageBase + entry->BeginAddress);

    return LookupUnwindEntry(module, address);
}
