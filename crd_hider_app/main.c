#include "main.h"
#include "functions.h"

// credits: https://github.com/zserge/tray
// stackoverflow

static struct tray  g_tray;
static int          g_enabled = 1;
static wchar_t      g_path[MAX_PATH];

static void toggle_cb(struct tray_menu* item)
{
    item->checked = !item->checked;

    g_enabled = item->checked;

    item->text = item->checked ? L"State: Enabled" : L"State: Disabled";

    tray_update(&g_tray);
}

static void about_cb(struct tray_menu* item)
{
    (void)item;

    MessageBox(NULL, L"https://github.com/rosko1337", L"crd_hider", MB_OK | MB_ICONINFORMATION);  

    tray_update(&g_tray);
}

static void quit_cb(struct tray_menu* item)
{
    (void)item;

    tray_exit();
}

static struct tray g_tray = {
    .icon = g_path,
    .menu =
        (struct tray_menu[]){
            {.text = L"State: Enabled", .checked = 1, .cb = toggle_cb},
            {.text = L"-"},
            {.text = L"About", .cb = about_cb},
            {.text = L"-"},
            {.text = L"Quit", .cb = quit_cb},
            {.text = NULL}},
};

#ifdef _DEBUG
int main()
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
#endif
{
    DWORD len = GetModuleFileName(NULL, g_path, MAX_PATH);
    if (len != 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        MessageBox(NULL, L"path buffer too small!", L"crd_hider", MB_OK | MB_ICONERROR);
        return ERR_BUFFER_TOO_SMALL;
    }

    HANDLE mutex = CreateMutex(NULL, FALSE, L"crd_app");
    if (mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        MessageBox(NULL, L"already running!", L"crd_hider", MB_OK | MB_ICONERROR);
        return ERR_ALREADY_RUNNING;
    }

    if (grant_privilege(L"SeDebugPrivilege") == FALSE) {
        MessageBox(NULL, L"failed to grant privilege!", L"crd_hider", MB_OK | MB_ICONERROR);
        return ERR_GRANT_PRIVILEGE;
    }

    if (tray_init(&g_tray) < 0) {
        MessageBox(NULL, L"failed to create tray object!", L"crd_hider", MB_OK | MB_ICONERROR);
        return ERR_INIT_TRAY_OBJ;
    }

    while (tray_loop(0) == 0)
    {
        static UINT64 time_last;
        UINT64 time_current = get_time();

        if (time_current - time_last > 50000) // 5s
        {
            HANDLE dll_mutex = CreateMutex(NULL, FALSE, L"crd_dll");
            if (dll_mutex != NULL && GetLastError() != ERROR_ALREADY_EXISTS)
            {
                // close mutex handle so crd_hider.dll can create it
                CloseHandle(dll_mutex);

                DWORD process_id = get_process_id(L"remoting_desktop.exe");
                if (g_enabled == TRUE && process_id != 0)
                {
                    BOOL injected = inject_dll(process_id);
                    if (injected == FALSE) {
                        MessageBox(NULL, L"cant inject dll!", L"crd_hider", MB_OK | MB_ICONERROR);
                    }
                }
            }

            if (dll_mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
                CloseHandle(dll_mutex);
            }

            time_last = time_current;
        }

        // reduce cpu usage
        Sleep(5);
    }

    if (mutex != NULL) {
        CloseHandle(mutex);
    }

    return ERROR_SUCCESS;
}