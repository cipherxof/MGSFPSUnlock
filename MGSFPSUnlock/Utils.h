#include <shlwapi.h>

DWORD GetGameVersion(HMODULE gameModule)
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
                        return verInfo->dwFileVersionMS;
                    }
                }
            }
        }
        delete[] verData;
    }

    return 0;
}