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

extern void PFD_StartupLogA(const char *text);

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

static bool PFD_IsWin9xCompat()
{
    return (GetVersion() & 0x80000000UL) != 0;
}

static bool PFD_WideStringToAnsiBuffer(const wchar_t *src, char *dst, size_t dstCount)
{
    if (!dst || dstCount == 0) return false;
    dst[0] = 0;
    if (!src) return true;
    int n = WideCharToMultiByte(CP_ACP, 0, src, -1, dst, (int)dstCount, NULL, NULL);
    if (n <= 0)
    {
        dst[0] = 0;
        return false;
    }
    dst[dstCount - 1] = 0;
    return true;
}

static bool PFD_WideMultiStringToAnsiBuffer(const wchar_t *src, char *dst, size_t dstCount)
{
    if (!dst || dstCount < 2) return false;
    dst[0] = dst[1] = 0;
    if (!src) return true;

    size_t used = 0;
    const wchar_t *cur = src;
    while (*cur)
    {
        int required = WideCharToMultiByte(CP_ACP, 0, cur, -1, NULL, 0, NULL, NULL);
        if (required <= 0 || used + (size_t)required + 1 > dstCount) return false;
        if (!WideCharToMultiByte(CP_ACP, 0, cur, -1, dst + used, required, NULL, NULL)) return false;
        used += (size_t)required;
        cur += wcslen(cur) + 1;
    }
    if (used >= dstCount) return false;
    dst[used] = 0; // second NUL terminator for the filter MULTI_SZ
    return true;
}

static bool PFD_AnsiMultiStringToWideBuffer(const char *src, wchar_t *dst, size_t dstCount)
{
    if (!src || !dst || dstCount < 2) return false;
    dst[0] = dst[1] = 0;

    size_t used = 0;
    const char *cur = src;
    while (*cur)
    {
        int required = MultiByteToWideChar(CP_ACP, 0, cur, -1, NULL, 0);
        if (required <= 0 || used + (size_t)required + 1 > dstCount) return false;
        if (!MultiByteToWideChar(CP_ACP, 0, cur, -1, dst + used, required)) return false;
        used += (size_t)required;
        cur += strlen(cur) + 1;
    }
    if (used >= dstCount) return false;
    dst[used] = 0;
    return true;
}

static void PFD_RecomputeOpenFileOffsets(OPENFILENAMEW *ofn)
{
    if (!ofn || !ofn->lpstrFile || !*ofn->lpstrFile) return;

    const wchar_t *first = ofn->lpstrFile;
    size_t firstLen = wcslen(first);
    const wchar_t *second = first + firstLen + 1;

    if (*second)
    {
        // Explorer-style multi-select: directory\0file1\0file2\0\0.
        ofn->nFileOffset = (WORD)(firstLen + 1);
        ofn->nFileExtension = 0;
        return;
    }

    size_t fileOffset = 0;
    size_t extOffset = 0;
    for (size_t i = 0; i < firstLen; ++i)
    {
        if (first[i] == L'\\' || first[i] == L'/')
        {
            fileOffset = i + 1;
            extOffset = 0;
        }
        else if (first[i] == L'.')
        {
            extOffset = i + 1;
        }
    }
    ofn->nFileOffset = (WORD)fileOffset;
    ofn->nFileExtension = (WORD)extOffset;
}

BOOL PFD_GetOpenFileNameCompat(OPENFILENAMEW *ofn)
{
    if (!ofn) return FALSE;

    if (!PFD_IsWin9xCompat())
        return GetOpenFileNameW(ofn);

    const DWORD bufferChars = ofn->nMaxFile ? ofn->nMaxFile : 1024;
    char *fileA = new char[bufferChars];
    char *filterA = new char[2048];
    char titleA[512] = { 0 };
    char initialDirA[MAX_PATH] = { 0 };
    char defExtA[64] = { 0 };
    memset(fileA, 0, bufferChars);
    memset(filterA, 0, 2048);

    if (ofn->lpstrFile && *ofn->lpstrFile)
        PFD_WideStringToAnsiBuffer(ofn->lpstrFile, fileA, bufferChars);
    if (!PFD_WideMultiStringToAnsiBuffer(ofn->lpstrFilter, filterA, 2048))
    {
        delete [] fileA;
        delete [] filterA;
        return FALSE;
    }
    PFD_WideStringToAnsiBuffer(ofn->lpstrTitle, titleA, sizeof(titleA));
    PFD_WideStringToAnsiBuffer(ofn->lpstrInitialDir, initialDirA, sizeof(initialDirA));
    PFD_WideStringToAnsiBuffer(ofn->lpstrDefExt, defExtA, sizeof(defExtA));

    OPENFILENAMEA a;
    ZeroMemory(&a, sizeof(a));
#ifdef OPENFILENAME_SIZE_VERSION_400
    a.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
    a.lStructSize = sizeof(OPENFILENAMEA);
#endif
    a.hwndOwner = ofn->hwndOwner;
    a.hInstance = ofn->hInstance;
    a.lpstrFilter = ofn->lpstrFilter ? filterA : NULL;
    a.nFilterIndex = ofn->nFilterIndex;
    a.lpstrFile = fileA;
    a.nMaxFile = bufferChars;
    a.lpstrInitialDir = (ofn->lpstrInitialDir && *ofn->lpstrInitialDir) ? initialDirA : NULL;
    a.lpstrTitle = (ofn->lpstrTitle && *ofn->lpstrTitle) ? titleA : NULL;
    a.Flags = ofn->Flags;
    a.lpstrDefExt = (ofn->lpstrDefExt && *ofn->lpstrDefExt) ? defExtA : NULL;
    a.lCustData = ofn->lCustData;
    a.lpfnHook = (LPOFNHOOKPROC)ofn->lpfnHook;

    SetLastError(ERROR_SUCCESS);
    BOOL ok = GetOpenFileNameA(&a);
    if (!ok)
    {
        DWORD dlgError = CommDlgExtendedError();
        if (dlgError != 0)
        {
            char line[160];
            wsprintfA(line, "GetOpenFileNameA failed on Win9x (CommDlgExtendedError=0x%08lX)\r\n",
                      (unsigned long)dlgError);
            PFD_StartupLogA(line);
        }
    }
    if (ok)
    {
        if (!PFD_AnsiMultiStringToWideBuffer(fileA, ofn->lpstrFile, ofn->nMaxFile))
            ok = FALSE;
        else
        {
            ofn->nFilterIndex = a.nFilterIndex;
            PFD_RecomputeOpenFileOffsets(ofn);
        }
    }

    delete [] fileA;
    delete [] filterA;
    return ok;
}

