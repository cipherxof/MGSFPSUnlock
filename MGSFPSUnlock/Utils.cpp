#include "Utils.h"

void GetGameType(HMODULE gameModule, GameType& result)
{
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileName(gameModule, exePath, MAX_PATH);
    WCHAR* filename = PathFindFileName(exePath);

    printf("%ls\n", filename);

    if (wcscmp(filename, L"METAL GEAR SOLID2.exe") == 0)
        result = GameType::MGS2;
    else if (wcscmp(filename, L"METAL GEAR SOLID3.exe") == 0)
        result = GameType::MGS3;
    else 
        result = GameType::Unknown;
}

uint64_t GetGameVersion(HMODULE gameModule)
{
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileName(gameModule, exePath, MAX_PATH);
    WCHAR* filename = PathFindFileName(exePath);

    if (wcsncmp(filename, L"launcher.exe", 13) == 0)
    {
        return 0;
    }

    DWORD  verHandle = 0;
    UINT   size = 0;
    LPBYTE lpBuffer = NULL;
    DWORD  verSize = GetFileVersionInfoSize(exePath, &verHandle);

    if (verSize != NULL)
    {
        LPSTR verData = new char[verSize];

        if (GetFileVersionInfo(exePath, verHandle, verSize, verData))
        {
            if (VerQueryValue(verData, L"\\", (VOID FAR * FAR*) & lpBuffer, &size))
            {
                if (size)
                {
                    VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
                    if (verInfo->dwSignature == 0xfeef04bd)
                    {
                        ULARGE_INTEGER version;
                        version.LowPart = verInfo->dwFileVersionLS;
                        version.HighPart = verInfo->dwFileVersionMS;

                        return version.QuadPart;
                    }
                }
            }
        }

        delete[] verData;
    }

    return 0;
}