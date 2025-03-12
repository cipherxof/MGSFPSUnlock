#include "Utils.h"
#include <windows.h>
#include <shlwapi.h>
#include <string>

void GetGameType(HMODULE gameModule, GameType& result)
{
    char exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameA(gameModule, exePath, _MAX_PATH);
    char* filename = PathFindFileNameA(exePath);
    printf("%s\n", filename);
    
    if (strcmp(filename, "METAL GEAR SOLID2.exe") == 0)
        result = GameType::MGS2;
    else if (strcmp(filename, "METAL GEAR SOLID3.exe") == 0)
        result = GameType::MGS3;
    else
        result = GameType::Unknown;
}

uint64_t GetGameVersion(HMODULE gameModule)
{
    char exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameA(gameModule, exePath, _MAX_PATH);
    char* filename = PathFindFileNameA(exePath);

    if (strncmp(filename, "launcher.exe", 13) == 0)
    {
        return 0;
    }

    DWORD verHandle = 0;
    UINT size = 0;
    LPBYTE lpBuffer = NULL;
    
    DWORD verSize = GetFileVersionInfoSizeA(exePath, &verHandle);
    
    if (verSize != 0)
    {
        BYTE* verData = new BYTE[verSize];

        if (GetFileVersionInfoA(exePath, verHandle, verSize, verData))
        {
            if (VerQueryValueA(verData, "\\", (VOID FAR* FAR*)&lpBuffer, &size))
            {
                if (size)
                {
                    VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
                    if (verInfo->dwSignature == 0xfeef04bd)
                    {
                        ULARGE_INTEGER version;
                        version.LowPart = verInfo->dwFileVersionLS;
                        version.HighPart = verInfo->dwFileVersionMS;
                        delete[] verData;
                        return version.QuadPart;
                    }
                }
            }
        }
        delete[] verData;
    }
    return 0;
}