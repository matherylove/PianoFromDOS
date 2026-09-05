/*************************************************************************************************
*
* File: Compat.h
*
* Description: PianoFromDOS Win9x/compiler compatibility helpers.
*
* Original Piano From Above copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#pragma once

#ifndef WINVER
#define WINVER 0x0410
#endif
#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0500
#endif

#include <windows.h>
#include <tchar.h>
#include <shlobj.h>
#include <commdlg.h>
#include <stdio.h>
#include <wchar.h>
#include <stdarg.h>
#include <string.h>
#include <string>

#ifndef _MSC_VER

#ifndef errno_t
typedef int errno_t;
#endif

/* MSVC's secure-CRT overloads are used throughout the 2010 codebase.  MinGW does
   not provide all of the same C++ template overloads, so keep the call sites and
   provide small bounded equivalents here. */
template <size_t N>
inline errno_t PFD_tcscpy_s(TCHAR (&dst)[N], const TCHAR *src)
{
    if (!src || N == 0) return 1;
    _tcsncpy(dst, src, N - 1);
    dst[N - 1] = 0;
    return 0;
}

template <size_t N>
inline errno_t PFD_tcsncpy_s(TCHAR (&dst)[N], const TCHAR *src, size_t count)
{
    if (!src || N == 0) return 1;
    size_t n = count < (N - 1) ? count : (N - 1);
    _tcsncpy(dst, src, n);
    dst[n] = 0;
    return 0;
}

template <size_t N>
inline errno_t PFD_tcscat_s(TCHAR (&dst)[N], const TCHAR *src)
{
    if (!src || N == 0) return 1;
    size_t used = _tcslen(dst);
    if (used >= N) return 1;
    _tcsncat(dst, src, N - used - 1);
    dst[N - 1] = 0;
    return 0;
}

template <size_t N>
inline errno_t PFD_strcat_s(char (&dst)[N], const char *src)
{
    if (!src || N == 0) return 1;
    size_t used = strlen(dst);
    if (used >= N) return 1;
    strncat(dst, src, N - used - 1);
    dst[N - 1] = 0;
    return 0;
}

template <size_t N>
inline errno_t PFD_wcscpy_s(wchar_t (&dst)[N], const wchar_t *src)
{
    if (!src || N == 0) return 1;
    wcsncpy(dst, src, N - 1);
    dst[N - 1] = 0;
    return 0;
}

template <size_t N>
inline int PFD_stprintf_s(TCHAR (&dst)[N], const TCHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef UNICODE
    int r = _vsnwprintf(dst, N - 1, fmt, ap);
#else
    int r = _vsnprintf(dst, N - 1, fmt, ap);
#endif
    va_end(ap);
    dst[N - 1] = 0;
    return r;
}

inline int PFD_stprintf_s(TCHAR *dst, size_t count, const TCHAR *fmt, ...)
{
    if (!dst || count == 0) return -1;
    va_list ap;
    va_start(ap, fmt);
#ifdef UNICODE
    int r = _vsnwprintf(dst, count - 1, fmt, ap);
#else
    int r = _vsnprintf(dst, count - 1, fmt, ap);
#endif
    va_end(ap);
    dst[count - 1] = 0;
    return r;
}

template <size_t N>
inline int PFD_swprintf_s(wchar_t (&dst)[N], const wchar_t *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = _vsnwprintf(dst, N - 1, fmt, ap);
    va_end(ap);
    dst[N - 1] = 0;
    return r;
}

inline errno_t PFD_fopen_s(FILE **fp, const char *filename, const char *mode)
{
    if (!fp) return 1;
    *fp = fopen(filename, mode);
    return *fp ? 0 : 1;
}

#define _tcscpy_s   PFD_tcscpy_s
#define _tcsncpy_s  PFD_tcsncpy_s
#define _tcscat_s   PFD_tcscat_s
#define strcat_s    PFD_strcat_s
#define wcscpy_s    PFD_wcscpy_s
#define _stprintf_s PFD_stprintf_s
#define swprintf_s  PFD_swprintf_s
#define fopen_s     PFD_fopen_s
#define sscanf_s    sscanf
#define _stscanf_s  _stscanf

#endif /* !_MSC_VER */

/* 32-bit compatibility aliases.  Get/SetWindowLongPtr are only separate entry
   points on 64-bit Windows; avoiding such imports is important on Windows 98. */
#if !defined(_WIN64)
#ifndef GetWindowLongPtr
#define GetWindowLongPtr GetWindowLong
#endif
#ifndef SetWindowLongPtr
#define SetWindowLongPtr SetWindowLong
#endif
#ifndef GWLP_WNDPROC
#define GWLP_WNDPROC GWL_WNDPROC
#endif
#ifndef GWLP_USERDATA
#define GWLP_USERDATA GWL_USERDATA
#endif
#ifndef DWLP_MSGRESULT
#define DWLP_MSGRESULT DWL_MSGRESULT
#endif
#endif

/* The style was added long after the Win9x common controls.  Zeroing it keeps
   the normal list-view styles without sending an unsupported extension bit. */
#if defined(PFD_TARGET_WIN98)
#ifdef LVS_EX_DOUBLEBUFFER
#undef LVS_EX_DOUBLEBUFFER
#endif
#define LVS_EX_DOUBLEBUFFER 0
#endif

void PFD_FillSolidRect(HDC hdc, const RECT *rc, COLORREF color);
bool PFD_GetSpecialFolderPathA(int csidl, char *outPath, size_t outCount);
bool PFD_GetSpecialFolderPathW(int csidl, wchar_t *outPath, size_t outCount);
std::string PFD_WidePathToAnsi(const wchar_t *path);
bool PFD_GetFileSizeW(const wchar_t *path, DWORD *fileSizeLow);
BOOL PFD_GetOpenFileNameCompat(OPENFILENAMEW *ofn);
