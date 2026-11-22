// Управляющее приложение для lab.exe.
// Сборка: gcc lab2.c -o lab2.exe -mwindows
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>

// Формат совпадает с SharedData из lab2_core.c (-1 == значение не задано)
typedef struct {
    LONG first;
    LONG second;
} SharedData;

enum {
    ID_INIT = 101, ID_START,
    ID_KEY, ID_SENDKEY,
    ID_SH1, ID_SH2, ID_SENDSHM,
    ID_STATUS
};

static HWND hInit, hKey, hSh1, hSh2, hStatus;
static HANDLE hProc = NULL;
static DWORD childPid = 0;

static void set_status(const wchar_t *fmt, ...) {
    wchar_t buf[256];
    va_list ap;
    va_start(ap, fmt);
    vswprintf(buf, 256, fmt, ap);
    va_end(ap);
    SetWindowTextW(hStatus, buf);
}

static int child_alive(void) {
    return hProc && WaitForSingleObject(hProc, 0) == WAIT_TIMEOUT;
}

// Возвращает 0..9, -1 для пустого поля, -2 для некорректного значения
static int read_digit(HWND edit) {
    wchar_t buf[16];
    GetWindowTextW(edit, buf, 16);
    if (buf[0] == L'\0')
        return -1;
    if (buf[0] >= L'0' && buf[0] <= L'9' && buf[1] == L'\0')
        return buf[0] - L'0';
    return -2;
}

static void start_child(void) {
    if (child_alive()) {
        set_status(L"Первая программа уже запущена (PID %lu)", childPid);
        return;
    }

    int init = read_digit(hInit);
    if (init == -2) {
        set_status(L"Начальная цифра должна быть от 0 до 9");
        return;
    }

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t *slash = wcsrchr(path, L'\\');
    if (slash)
        *(slash + 1) = L'\0';

    wchar_t cmd[MAX_PATH + 32];
    if (init >= 0)
        swprintf(cmd, MAX_PATH + 32, L"\"%lslab.exe\" %d", path, init);
    else
        swprintf(cmd, MAX_PATH + 32, L"\"%lslab.exe\"", path);

    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        set_status(L"Не удалось запустить lab.exe, ошибка %lu", GetLastError());
        return;
    }
    CloseHandle(pi.hThread);
    if (hProc)
        CloseHandle(hProc);
    hProc = pi.hProcess;
    childPid = pi.dwProcessId;
    set_status(L"Запущена lab.exe, PID %lu", childPid);
}

// Отправляет в консоль первой программы сообщения о нажатии и отпускании клавиши
static void send_key(void) {
    if (!child_alive()) {
        set_status(L"Сначала запустите первую программу");
        return;
    }
    int d = read_digit(hKey);
    if (d < 0) {
        set_status(L"Введите цифру от 0 до 9 для отправки");
        return;
    }

    FreeConsole();
    if (!AttachConsole(childPid)) {
        set_status(L"AttachConsole: ошибка %lu", GetLastError());
        return;
    }

    HANDLE conIn = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (conIn == INVALID_HANDLE_VALUE) {
        set_status(L"CONIN$: ошибка %lu", GetLastError());
        FreeConsole();
        return;
    }

    WCHAR ch = (WCHAR)(L'0' + d);
    INPUT_RECORD rec[2] = { 0 };
    for (int i = 0; i < 2; i++) {
        rec[i].EventType = KEY_EVENT;
        rec[i].Event.KeyEvent.bKeyDown = (i == 0);
        rec[i].Event.KeyEvent.wRepeatCount = 1;
        rec[i].Event.KeyEvent.wVirtualKeyCode = (WORD)ch;
        rec[i].Event.KeyEvent.wVirtualScanCode = (WORD)MapVirtualKeyW(ch, MAPVK_VK_TO_VSC);
        rec[i].Event.KeyEvent.uChar.UnicodeChar = ch;
    }

    DWORD written = 0;
    BOOL ok = WriteConsoleInputW(conIn, rec, 2, &written);
    DWORD err = GetLastError();
    CloseHandle(conIn);
    FreeConsole();

    if (ok)
        set_status(L"Клавиша %d отправлена", d);
    else
        set_status(L"WriteConsoleInput: ошибка %lu", err);
}

// Записывает значения в разделяемую память и сигнализирует первой программе
static void send_shared(void) {
    if (!child_alive()) {
        set_status(L"Сначала запустите первую программу");
        return;
    }
    int a = read_digit(hSh1);
    int b = read_digit(hSh2);
    if (a == -2 || b == -2) {
        set_status(L"Значения должны быть цифрами 0-9 или пустыми");
        return;
    }

    char memName[64], evName[64];
    snprintf(memName, sizeof memName, "Local\\OSLab_%lu_Mem", childPid);
    snprintf(evName, sizeof evName, "Local\\OSLab_%lu_Event", childPid);

    HANDLE hMap = NULL, hEvent = NULL;
    for (int i = 0; i < 20 && !(hMap && hEvent); i++) { // первая программа могла ещё не создать объекты
        if (!hMap)
            hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, memName);
        if (!hEvent)
            hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, evName);
        if (!(hMap && hEvent))
            Sleep(100);
    }
    if (!hMap || !hEvent) {
        set_status(L"Не удалось открыть разделяемую память, ошибка %lu", GetLastError());
        if (hMap) CloseHandle(hMap);
        if (hEvent) CloseHandle(hEvent);
        return;
    }

    SharedData *shared = (SharedData *)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (!shared) {
        set_status(L"MapViewOfFile: ошибка %lu", GetLastError());
    } else {
        InterlockedExchange(&shared->first, a);
        InterlockedExchange(&shared->second, b);
        SetEvent(hEvent);
        UnmapViewOfFile(shared);
        set_status(L"В разделяемую память записано: %d, %d", a, b);
    }
    CloseHandle(hMap);
    CloseHandle(hEvent);
}

static HWND make(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, HWND parent, int id) {
    return CreateWindowExW(cls[0] == L'E' ? WS_EX_CLIENTEDGE : 0, cls, text, WS_CHILD | WS_VISIBLE | style,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND c;

        make(L"STATIC", L"Начальная цифра (пусто - нет):", 0, 10, 14, 220, 20, hwnd, 0);
        hInit = make(L"EDIT", L"", WS_TABSTOP | ES_NUMBER, 235, 10, 40, 24, hwnd, ID_INIT);
        SendMessageW(hInit, EM_LIMITTEXT, 1, 0);
        make(L"BUTTON", L"Запустить lab.exe", WS_TABSTOP | BS_PUSHBUTTON, 290, 9, 170, 26, hwnd, ID_START);

        make(L"STATIC", L"Клавиша для отправки (0-9):", 0, 10, 54, 220, 20, hwnd, 0);
        hKey = make(L"EDIT", L"", WS_TABSTOP | ES_NUMBER, 235, 50, 40, 24, hwnd, ID_KEY);
        SendMessageW(hKey, EM_LIMITTEXT, 1, 0);
        make(L"BUTTON", L"Отправить клавишу", WS_TABSTOP | BS_PUSHBUTTON, 290, 49, 170, 26, hwnd, ID_SENDKEY);

        make(L"STATIC", L"Разделяемая память: first / second", 0, 10, 94, 220, 20, hwnd, 0);
        hSh1 = make(L"EDIT", L"", WS_TABSTOP | ES_NUMBER, 235, 90, 40, 24, hwnd, ID_SH1);
        hSh2 = make(L"EDIT", L"", WS_TABSTOP | ES_NUMBER, 285, 90, 40, 24, hwnd, ID_SH2);
        SendMessageW(hSh1, EM_LIMITTEXT, 1, 0);
        SendMessageW(hSh2, EM_LIMITTEXT, 1, 0);
        make(L"BUTTON", L"Записать в память", WS_TABSTOP | BS_PUSHBUTTON, 335, 89, 125, 26, hwnd, ID_SENDSHM);

        hStatus = make(L"STATIC", L"Готово", SS_LEFT, 10, 135, 450, 20, hwnd, ID_STATUS);

        for (c = GetWindow(hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
            SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED) {
            switch (LOWORD(wp)) {
            case ID_START: start_child(); break;
            case ID_SENDKEY: send_key(); break;
            case ID_SENDSHM: send_shared(); break;
            }
        }
        return 0;
    case WM_DESTROY:
        if (hProc)
            CloseHandle(hProc);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int nShow) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"OSLab2Window";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"OSLab2Window", L"Управление lab.exe",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, 490, 210, NULL, NULL, hInst, NULL);
    if (!hwnd)
        return 1;
    ShowWindow(hwnd, nShow);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    return (int)m.wParam;
}
