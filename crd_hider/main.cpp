#include "main.h"

#include "detours/detours.h"
#pragma comment(lib, "detours/detours.lib")

HANDLE  g_mutex = nullptr;
bool    g_unloading = false;

// https://stackoverflow.com/questions/20162359/c-best-way-to-get-window-handle-of-the-only-window-from-a-process-by-process/20163705#20163705

struct EnumData {
    DWORD dwProcessId;
    HWND hWnd;
};

BOOL CALLBACK EnumProc(HWND hWnd, LPARAM lParam) {
    // Retrieve storage location for communication data
    EnumData& ed = *(EnumData*)lParam;
    DWORD dwProcessId = 0x0;
    // Query process ID for hWnd
    GetWindowThreadProcessId(hWnd, &dwProcessId);
    // Apply filter - if you want to implement additional restrictions,
    // this is the place to do so.
    if (ed.dwProcessId == dwProcessId) {
        // Found a window matching the process ID
        ed.hWnd = hWnd;
        // Report success
        SetLastError(ERROR_SUCCESS);
        // Stop enumeration
        return FALSE;
    }
    // Continue enumeration
}

HWND FindWindowFromProcessId(DWORD dwProcessId) {
    EnumData ed = { dwProcessId, 0 };
    if (!EnumWindows(EnumProc, (LPARAM)&ed) &&
        ((GetLastError() == ERROR_SUCCESS) || ed.hWnd != 0)) {
        return ed.hWnd;
    }
    return NULL;
}

// структура дл€ хранени€ необходимых указателей и адресов
struct user32_dll {
    HMODULE dll;
    FARPROC Orignal_ShowWindow;
    FARPROC Orignal_AnimateWindow;
    FARPROC Orignal_IsWindowVisible;
} user32;

using ShowWindowFn = BOOL(__stdcall*)(HWND, int);
BOOL __stdcall hooked_ShowWindow(HWND hWnd, int nCmdShow)
{
    // объ€вление оригинальной функции ShowWindow
    auto original = reinterpret_cast<ShowWindowFn>(user32.Orignal_ShowWindow);

    // подмена значени€ аргумента
    if (nCmdShow == SW_SHOW)
        nCmdShow = SW_HIDE;

    // вызов оригинальной функции
    auto return_value = original(hWnd, nCmdShow);

    // возвращаем значение вызова
    return return_value;
}

using AnimateWindowFn = BOOL(__stdcall*)(HWND, DWORD, DWORD);
BOOL __stdcall hooked_AnimateWindow(HWND hWnd, DWORD dwTime, DWORD dwFlags)
{
    // объ€вление оригинальной функции AnimateWindow
    auto original = reinterpret_cast<AnimateWindowFn>(user32.Orignal_AnimateWindow);

    // подмена возвращаемого значени€ вызова функции, без вызова оригинала, иначе - краш. подмена флагов тоже ведет к крашу.
    auto return_value = TRUE;

    // возвращаем значение вызова
    return return_value;
}

using IsWindowVisibleFn = BOOL(__stdcall*)(HWND);
BOOL __stdcall hooked_IsWindowVisible(HWND hWnd)
{
    // объ€вление оригинальной функции IsWindowVisible
    auto original = reinterpret_cast<IsWindowVisibleFn>(user32.Orignal_IsWindowVisible);

    // вызов оригинальной функции
    auto return_value = original(hWnd);

    // подмена возвращенного значени€ вызова функции
    return_value = TRUE;

    // возвращаем значение вызова
    return return_value;
}

/*struct gdi32_dll {
    HMODULE dll;
    FARPROC Orignal_ScriptTextOut;
} gdi32;

using ScriptTextOutFn = HRESULT(__stdcall*)(const HDC, void*, int, int, UINT, const RECT*, const void*, WCHAR*, int, const WORD*, int, const int*, const int*, const void*);
HRESULT __stdcall hooked_ScriptTextOut(const HDC hdc, void* psc, int x, int y, UINT fuOptions, const RECT* lprc, const void* psa, WCHAR* pwcReserved, int iReserved, const WORD* pwGlyphs, int cGlyphs, const int* piAdvance, const int* piJustify, const void* pGoffset)
{
    auto original = reinterpret_cast<ScriptTextOutFn>(gdi32.Orignal_ScriptTextOut);
    y = 10;
    auto return_value = original(hdc, psc, x, y, fuOptions, lprc, psa, pwcReserved, iReserved, pwGlyphs, cGlyphs, piAdvance, piJustify, pGoffset);
    return return_value;
}*/

void main_thread(HMODULE hModule)
{
    g_mutex = CreateMutex(NULL, FALSE, L"crd_dll");
    if (g_mutex != 0 && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBox(0, L"ogo", L"ogo", MB_OK | MB_ICONERROR);
        return;
    }

    // ждем загрузки user32.dll, если он еще на загрузилс€
    while (!GetModuleHandle(L"user32.dll"))
        Sleep(100);

    // получаем указатель на user32.dll
    user32.dll = GetModuleHandle(L"user32.dll");
    if (user32.dll == nullptr) // провер€ем получили ли мы указатель, и если нет, то ->
    {
        // ждем 3 секунды
        Sleep(3000);
        // освобождаем библиотеку и завершаем поток
        FreeLibraryAndExitThread(hModule, 0);
    }

    // ищем указатель окна по id текущего процесса
    auto hwnd = FindWindowFromProcessId(GetCurrentProcessId());   
    // если найденное окно видно
    if (IsWindowVisible(hwnd))
        // скрываем найденное окно с анимацией
        AnimateWindow(hwnd, 0xC8, AW_BLEND | AW_HIDE);
    // полностью скрываем окно
    ShowWindow(hwnd, 0);

    // из библиотеки user32.dll получаем адрес функции ShowWindow
    user32.Orignal_ShowWindow = GetProcAddress(user32.dll, "ShowWindow");
    user32.Orignal_AnimateWindow = GetProcAddress(user32.dll, "AnimateWindow");
    user32.Orignal_IsWindowVisible = GetProcAddress(user32.dll, "IsWindowVisible");

    //gdi32.dll = GetModuleHandle(L"gdi32full.dll");
    //gdi32.Orignal_ScriptTextOut = GetProcAddress(gdi32.dll, "ScriptTextOut");

    // перехватываем функции ShowWindow, AnimateWindow и IsWindowVisible дл€ управлени€ их входными и выходными данными 
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&reinterpret_cast<void*&>(user32.Orignal_ShowWindow), &hooked_ShowWindow);
    DetourAttach(&reinterpret_cast<void*&>(user32.Orignal_AnimateWindow), &hooked_AnimateWindow);
    DetourAttach(&reinterpret_cast<void*&>(user32.Orignal_IsWindowVisible), &hooked_IsWindowVisible);
    //DetourAttach(&reinterpret_cast<void*&>(gdi32.Orignal_ScriptTextOut), &hooked_ScriptTextOut);
    DetourTransactionCommit();

    while (!g_unloading) {
        Sleep(1000);
    }
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)main_thread, hModule, 0, 0);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        g_unloading = true;
        Sleep(100);

        if (g_mutex != 0)
            CloseHandle(g_mutex);

        // отдаем управление функци€ми ShowWindow и AnimateWindow обратно
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&reinterpret_cast<void*&>(user32.Orignal_ShowWindow), &hooked_ShowWindow);
        DetourDetach(&reinterpret_cast<void*&>(user32.Orignal_AnimateWindow), &hooked_AnimateWindow);
        DetourDetach(&reinterpret_cast<void*&>(user32.Orignal_IsWindowVisible), &hooked_IsWindowVisible);
        //DetourDetach(&reinterpret_cast<void*&>(gdi32.Orignal_ScriptTextOut), &hooked_ScriptTextOut);
        DetourTransactionCommit();

        // освобождаем библиотеку и завершаем поток
        FreeLibraryAndExitThread(hModule, 0);
    }

    return TRUE;
}