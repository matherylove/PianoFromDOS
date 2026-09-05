/*************************************************************************************************
*
* File: Compat.cpp
*
* Description: PianoFromDOS Win9x/compiler compatibility helpers.
*
* Original Piano From Above copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#include "Compat.h"

#include <stdlib.h>

void PFD_FillSolidRect(HDC hdc, const RECT *rc, COLORREF color)
{
    if (!hdc || !rc) return;
    HBRUSH brush = CreateSolidBrush(color);
    if (!brush) return;
    FillRect(hdc, rc, brush);
    DeleteObject(brush);
}

bool PFD_GetSpecialFolderPathA(int csidl, char *outPath, size_t outCount)
{
    if (!outPath || outCount == 0) return false;
    outPath[0] = 0;

    char temp[MAX_PATH] = { 0 };
    LPITEMIDLIST pidl = NULL;
    if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, csidl, &pidl)) && pidl)
    {
        BOOL ok = SHGetPathFromIDListA(pidl, temp);
        CoTaskMemFree(pidl);
        if (ok)
        {
            strncpy(outPath, temp, outCount - 1);
            outPath[outCount - 1] = 0;
            return true;
        }
    }

    /* APPDATA is the only special folder that is essential to startup.  This
       fallback also helps minimally installed Win98 systems with older shell DLLs. */
    if (csidl == CSIDL_APPDATA)
    {
        const char *appData = getenv("APPDATA");
        if (appData && *appData)
        {
            strncpy(outPath, appData, outCount - 1);
            outPath[outCount - 1] = 0;
            return true;
        }

        const char *windir = getenv("WINDIR");
        if (windir && *windir)
        {
            _snprintf(outPath, outCount - 1, "%s\\Application Data", windir);
            outPath[outCount - 1] = 0;
            return true;
        }
    }

    return false;
}

bool PFD_GetSpecialFolderPathW(int csidl, wchar_t *outPath, size_t outCount)
{
    if (!outPath || outCount == 0) return false;
    outPath[0] = 0;
    char temp[MAX_PATH] = { 0 };
    if (!PFD_GetSpecialFolderPathA(csidl, temp, sizeof(temp))) return false;

    int converted = MultiByteToWideChar(CP_ACP, 0, temp, -1, outPath, (int)outCount);
    if (!converted)
    {
        outPath[0] = 0;
        return false;
    }
    outPath[outCount - 1] = 0;
    return true;
}

std::string PFD_WidePathToAnsi(const wchar_t *path)
{
    if (!path) return std::string();

    int required = WideCharToMultiByte(CP_ACP, 0, path, -1, NULL, 0, NULL, NULL);
    if (required <= 0) return std::string();

    std::string result;
    result.resize((size_t)required);
    if (!WideCharToMultiByte(CP_ACP, 0, path, -1, &result[0], required, NULL, NULL))
        return std::string();
    if (!result.empty() && result[result.size() - 1] == '\0') result.resize(result.size() - 1);
    return result;
}

bool PFD_GetFileSizeW(const wchar_t *path, DWORD *fileSizeLow)
{
    if (!fileSizeLow) return false;
    *fileSizeLow = 0;

    std::string ansiPath = PFD_WidePathToAnsi(path);
    if (ansiPath.empty()) return false;

    WIN32_FIND_DATAA data;
    HANDLE hFind = FindFirstFileA(ansiPath.c_str(), &data);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    FindClose(hFind);

    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return false;
    *fileSizeLow = data.nFileSizeLow;
    return true;
}
