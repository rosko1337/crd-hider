#ifndef MAIN_H
#define MAIN_H

#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <TlHelp32.h>

#include "resource.h"

#if defined (_WIN32) || defined (_WIN64)
#define TRAY_WINAPI 1
#endif

#include "tray.h"

#if TRAY_WINAPI
#define TRAY_ICON1 L"icon.ico"
#endif

// dll file name
#define CRD_HIDER_DLL L"crd_hider.dll"

// err codes
#define ERR_BUFFER_TOO_SMALL 1
#define ERR_ALREADY_RUNNING  2
#define ERR_GRANT_PRIVILEGE  3
#define ERR_INIT_TRAY_OBJ    4

#endif /* MAIN_H */