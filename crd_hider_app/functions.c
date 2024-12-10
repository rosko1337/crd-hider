#include "functions.h"

BOOL grant_privilege(PWSTR privilege)
{
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken)) {
        return FALSE;
    }

    TOKEN_PRIVILEGES TokenPrivileges = { 0 };
    TokenPrivileges.PrivilegeCount = 1;
    TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValue(NULL, privilege, &TokenPrivileges.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return FALSE;
    }

    if (!AdjustTokenPrivileges(hToken, FALSE, &TokenPrivileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);

    return TRUE;
}

DWORD get_process_id(PWSTR process_name)
{
    DWORD process_id = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pE;
        pE.dwSize = sizeof(pE);

        if (Process32First(hSnap, &pE))
        {
            if (!pE.th32ProcessID)
                Process32Next(hSnap, &pE);
            do
            {
                if (!wcscmp(pE.szExeFile, process_name))
                {
                    process_id = pE.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &pE));
        }
    }
    CloseHandle(hSnap);
    return process_id;
}

BOOL inject_dll(DWORD process_id)
{
    wchar_t dll_name[] = CRD_HIDER_DLL;
    wchar_t dll_path[MAX_PATH];

    DWORD dwFullPathResult = GetFullPathName(dll_name, MAX_PATH, dll_path, NULL);
    if (dwFullPathResult == 0) {
        return FALSE;
    }

    HANDLE hTargetProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);
    if (hTargetProcess == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    LPVOID lpPathAddress = VirtualAllocEx(hTargetProcess, NULL, sizeof(dll_path), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (lpPathAddress == NULL) {
        return FALSE;
    }

    DWORD dwWriteResult = WriteProcessMemory(hTargetProcess, lpPathAddress, dll_path, sizeof(dll_path), NULL);
    if (dwWriteResult == NULL) {
        return FALSE;
    }

    HMODULE hModule = GetModuleHandle(L"kernel32.dll");
    if (hModule == INVALID_HANDLE_VALUE || hModule == NULL) {
        return FALSE;
    }

    FARPROC lpFunctionAddress = GetProcAddress(hModule, "LoadLibraryW");
    if (lpFunctionAddress == NULL) {
        return FALSE;
    }

    HANDLE hThreadCreationResult = CreateRemoteThread(hTargetProcess, NULL, 0, (LPTHREAD_START_ROUTINE)lpFunctionAddress, lpPathAddress, 0, NULL);
    if (hThreadCreationResult == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    if (hThreadCreationResult != NULL) {
        WaitForSingleObject(hThreadCreationResult, 1000);
    }

    if (hThreadCreationResult != NULL) {
        CloseHandle(hThreadCreationResult);
    }

    if (lpPathAddress != NULL) {
        VirtualFreeEx(hTargetProcess, lpPathAddress, 0, MEM_RELEASE);
    }

    return TRUE;
}

UINT64 get_time()
{
    static int				init = 0;
    static ULARGE_INTEGER	init_time;
    FILETIME				ft;
    ULARGE_INTEGER			time_now;

    GetSystemTimeAsFileTime(&ft);
    time_now.LowPart = ft.dwLowDateTime;
    time_now.HighPart = ft.dwHighDateTime;

    time_now.QuadPart /= (UINT64)1000; /* convert to 100 usec */

    if (!init) {
        init_time.QuadPart = time_now.QuadPart - 1000 * 10;
        init = 1;
    }

    return (UINT64)(time_now.QuadPart - init_time.QuadPart);
}