/*
 * Beacon Object Files (BOF) — Cobalt Strike 4.x
 * Vendored from https://github.com/Cobalt-Strike/bof_template
 */
#ifndef _BEACON_H_
#define _BEACON_H_
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	char *original;
	char *buffer;
	int length;
	int size;
} datap;

DECLSPEC_IMPORT void BeaconDataParse(datap *parser, char *buffer, int size);
DECLSPEC_IMPORT char *BeaconDataPtr(datap *parser, int size);
DECLSPEC_IMPORT int BeaconDataInt(datap *parser);
DECLSPEC_IMPORT short BeaconDataShort(datap *parser);
DECLSPEC_IMPORT int BeaconDataLength(datap *parser);
DECLSPEC_IMPORT char *BeaconDataExtract(datap *parser, int *size);

typedef struct {
	char *original;
	char *buffer;
	int length;
	int size;
} formatp;

DECLSPEC_IMPORT void BeaconFormatAlloc(formatp *format, int maxsz);
DECLSPEC_IMPORT void BeaconFormatReset(formatp *format);
DECLSPEC_IMPORT void BeaconFormatAppend(formatp *format, const char *text, int len);
DECLSPEC_IMPORT void BeaconFormatPrintf(formatp *format, const char *fmt, ...);
DECLSPEC_IMPORT char *BeaconFormatToString(formatp *format, int *size);
DECLSPEC_IMPORT void BeaconFormatFree(formatp *format);
DECLSPEC_IMPORT void BeaconFormatInt(formatp *format, int value);

#define CALLBACK_OUTPUT      0x0
#define CALLBACK_OUTPUT_OEM  0x1e
#define CALLBACK_OUTPUT_UTF8 0x20
#define CALLBACK_ERROR       0x0d
#define CALLBACK_CUSTOM      0x1000
#define CALLBACK_CUSTOM_LAST 0x13ff

DECLSPEC_IMPORT void BeaconOutput(int type, const char *data, int len);
DECLSPEC_IMPORT void BeaconPrintf(int type, const char *fmt, ...);
DECLSPEC_IMPORT BOOL BeaconDownload(const char *filename, const char *buffer, unsigned int length);

DECLSPEC_IMPORT BOOL BeaconUseToken(HANDLE token);
DECLSPEC_IMPORT void BeaconRevertToken(void);
DECLSPEC_IMPORT BOOL BeaconIsAdmin(void);

DECLSPEC_IMPORT void BeaconGetSpawnTo(BOOL x86, char *buffer, int length);
DECLSPEC_IMPORT void BeaconInjectProcess(HANDLE hProc, int pid, char *payload, int p_len, int p_offset, char *arg, int a_len);
DECLSPEC_IMPORT void BeaconInjectTemporaryProcess(PROCESS_INFORMATION *pInfo, char *payload, int p_len, int p_offset, char *arg, int a_len);
DECLSPEC_IMPORT BOOL BeaconSpawnTemporaryProcess(BOOL x86, BOOL ignoreToken, STARTUPINFO *si, PROCESS_INFORMATION *pInfo);
DECLSPEC_IMPORT void BeaconCleanupProcess(PROCESS_INFORMATION *pInfo);

DECLSPEC_IMPORT BOOL toWideChar(char *src, wchar_t *dst, int max);

typedef struct {
	char *ptr;
	size_t size;
} HEAP_RECORD;
#define MASK_SIZE 13

DECLSPEC_IMPORT BOOL BeaconAddValue(const char *key, void *ptr);
DECLSPEC_IMPORT void *BeaconGetValue(const char *key);
DECLSPEC_IMPORT BOOL BeaconRemoveValue(const char *key);

#ifdef __cplusplus
}
#endif
#endif
