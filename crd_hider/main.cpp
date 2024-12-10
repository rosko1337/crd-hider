#include "main.h"

#include "safetyhook\safetyhook.hpp"

std::atomic_bool    g_unloading = false;
HANDLE              g_mutex = nullptr;
SafetyHookInline    g_hookShowWindow{};
SafetyHookInline    g_hookAnimateWindow{};
SafetyHookInline    g_hookIsWindowVisible{};

// https://stackoverflow.com/questions/20162359/c-best-way-to-get-window-handle-of-the-only-window-from-a-process-by-process/20163705#20163705
namespace helpers
{
    struct EnumData {
        DWORD dwProcessId;
        HWND hWnd;
    };

    BOOL CALLBACK EnumProc(HWND hWnd, LPARAM lParam) {
        EnumData& ed = *(EnumData*)lParam;
        DWORD dwProcessId = 0x0;
        GetWindowThreadProcessId(hWnd, &dwProcessId);
        if (ed.dwProcessId == dwProcessId) {
            ed.hWnd = hWnd;
            SetLastError(ERROR_SUCCESS);
            return FALSE;
        }
        return TRUE;
    }

    HWND FindWindowFromProcessId(DWORD dwProcessId) {
        EnumData ed = { dwProcessId, 0 };
        if (!EnumWindows(EnumProc, (LPARAM)&ed) &&
            ((GetLastError() == ERROR_SUCCESS) || ed.hWnd != 0)) {
            return ed.hWnd;
        }
        return NULL;
    }
}

BOOL __stdcall hooked_ShowWindow(HWND hWnd, int nCmdShow)
{
    // replace input value
    if (nCmdShow == SW_SHOW)
        nCmdShow = SW_HIDE;

    // call original and return a value
    return g_hookShowWindow.stdcall<BOOL, HWND, int>(hWnd, nCmdShow);
}

BOOL __stdcall hooked_AnimateWindow(HWND hWnd, DWORD dwTime, DWORD dwFlags)
{
    // just return a value
    return TRUE;
}

BOOL __stdcall hooked_IsWindowVisible(HWND hWnd)
{
    // call original
    g_hookIsWindowVisible.stdcall<BOOL, HWND>(hWnd);

    // and return a value
    return TRUE;
}

void main_thread(HMODULE hModule)
{
    // we need only one instance
    g_mutex = CreateMutex(NULL, FALSE, L"crd_dll");
    if (g_mutex != 0 && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // throw a strange messagebox
        MessageBox(0, L"crd_hider.dll already loaded?", L"crd_hider", MB_OK | MB_ICONERROR);
        // exit
        return;
    }

    // waiting for user32.dll to load, if it is not loaded
    while (!GetModuleHandle(L"user32.dll"))
        Sleep(100);

    // get user32.dll address
    auto user32_dll = GetModuleHandle(L"user32.dll");
    if (user32_dll == nullptr) // if cant get user32.dll address (?) -> exit
    {
        // throw a stupid messagebox
        MessageBox(0, L"GetModuleHandle failed", L"crd_hider", MB_OK | MB_ICONERROR);
        // exit
        return;
    }

    // get hwnd by process id
    auto hwnd = helpers::FindWindowFromProcessId(GetCurrentProcessId());

    // if window is visible
    if (IsWindowVisible(hwnd))
        // hide a window with animation
        AnimateWindow(hwnd, 0xC8, AW_BLEND | AW_HIDE);

    // hide a window
    ShowWindow(hwnd, SW_HIDE);

    // from user32.dll get addr of ShowWindow, AnimateWindow and IsWindowVisible
    auto show_window_fn = GetProcAddress(user32_dll, "ShowWindow");
    auto animate_window_fn = GetProcAddress(user32_dll, "AnimateWindow");
    auto is_window_visible_fn = GetProcAddress(user32_dll, "IsWindowVisible");

    if (show_window_fn == nullptr || animate_window_fn == nullptr || is_window_visible_fn == nullptr)
    {
        // throw a simple messagebox
        MessageBox(0, L"GetProcAddress failed", L"crd_hider", MB_OK | MB_ICONERROR);
        // exit
        return;
    }

    // hook them
    g_hookShowWindow = safetyhook::create_inline(reinterpret_cast<void*>(show_window_fn), reinterpret_cast<void*>(hooked_ShowWindow)); //safetyhook::create_inline(show_window_fn, hooked_ShowWindow);
    g_hookAnimateWindow = safetyhook::create_inline(reinterpret_cast<void*>(animate_window_fn), reinterpret_cast<void*>(hooked_AnimateWindow)); //safetyhook::create_inline(animate_window_fn, hooked_AnimateWindow);
    g_hookIsWindowVisible = safetyhook::create_inline(reinterpret_cast<void*>(is_window_visible_fn), reinterpret_cast<void*>(hooked_IsWindowVisible)); //safetyhook::create_inline(is_window_visible_fn, hooked_IsWindowVisible);

    while (!g_unloading) {
        Sleep(3000);
    }
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);

        if (CreateThread(0, 0, (LPTHREAD_START_ROUTINE)main_thread, hModule, 0, 0) == NULL)
        {
            // just throw a messagebox
            MessageBox(0, L"CreateThread failed", L"crd_hider", MB_OK | MB_ICONERROR);
            // exit
            FreeLibraryAndExitThread(hModule, 0);
        }
        break;
    case DLL_PROCESS_DETACH:
        g_unloading = true;
        Sleep(3000);

        if (g_mutex != 0)
            CloseHandle(g_mutex);

        // unhook
        g_hookShowWindow = {};
        g_hookAnimateWindow = {};
        g_hookIsWindowVisible = {};

        // exit
        FreeLibraryAndExitThread(hModule, 0);
        break;
    }

    return TRUE;
}