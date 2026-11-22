#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define VK_ESCAPE_CODE 0x1B

// Структура в разделяемой памяти (-1 == значение не задано).
// Тот же формат использует lab2.c.
typedef struct {
    LONG first;
    LONG second;
} SharedData;

static int first = -1; // ожидающая первая цифра, -1 == None

static void handle_digit(int digit) {
    if (first < 0) {
        first = digit;
        printf("Первая цифра: %d\n", digit);
    } else {
        printf("Вторая цифра: %d\n", digit);
        printf("%d + %d = %d\n", first, digit, first + digit);
        first = -1;
    }
    fflush(stdout);
}

// Читает значения по адресу разделяемой памяти и отображает их
static void handle_shared(const volatile SharedData *shared) {
    int a = (int)shared->first;
    int b = (int)shared->second;

    printf("Из разделяемой памяти: first=%d, second=%d\n", a, b);
    if (a >= 0 && a <= 9)
        handle_digit(a);
    if (b >= 0 && b <= 9)
        handle_digit(b);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    // Начальное состояние из командной строки: lab.exe [первая_цифра]
    if (argc > 1 && argv[1][0] >= '0' && argv[1][0] <= '9' && argv[1][1] == '\0')
        first = argv[1][0] - '0';

    HANDLE stdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (stdInput == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "GetStdHandle failed: %lu\n", GetLastError());
        return 1;
    }

    // Имена объектов зависят от PID, чтобы lab2.exe мог найти нужный процесс
    char memName[64], evName[64];
    DWORD pid = GetCurrentProcessId();
    snprintf(memName, sizeof memName, "Local\\OSLab_%lu_Mem", pid);
    snprintf(evName, sizeof evName, "Local\\OSLab_%lu_Event", pid);

    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                     0, sizeof(SharedData), memName);
    SharedData *shared = hMap ? (SharedData *)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData)) : NULL;
    HANDLE hEvent = CreateEventA(NULL, FALSE, FALSE, evName); // автосброс
    if (!shared || !hEvent) {
        fprintf(stderr, "Shared memory init failed: %lu\n", GetLastError());
        return 1;
    }
    shared->first = -1;
    shared->second = -1;

    printf("PID: %lu\n", pid);
    printf("Нажмите клавишу от 0 до 9 (M - прочитать разделяемую память, Esc - выход)\n");
    if (first >= 0)
        printf("Начальное состояние, первая цифра: %d\n", first);
    fflush(stdout);

    HANDLE waitFor[2] = { stdInput, hEvent };
    INPUT_RECORD record;
    DWORD count;
    int running = 1;

    while (running) {
        DWORD w = WaitForMultipleObjects(2, waitFor, FALSE, INFINITE);

        if (w == WAIT_OBJECT_0 + 1) { // lab2 записал данные в разделяемую память
            handle_shared(shared);
            continue;
        }
        if (w != WAIT_OBJECT_0)
            break;

        if (!ReadConsoleInputW(stdInput, &record, 1, &count))
            break;

        // нам интересны только нажатия клавиш
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
            continue;

        KEY_EVENT_RECORD *key = &record.Event.KeyEvent;

        if (key->wVirtualKeyCode == VK_ESCAPE_CODE) {
            running = 0;
            continue;
        }

        WCHAR ch = key->uChar.UnicodeChar;
        if (ch == L'm' || ch == L'M') {
            handle_shared(shared);
            continue;
        }
        if (ch < L'0' || ch > L'9')
            continue;

        handle_digit(ch - L'0');
    }

    UnmapViewOfFile(shared);
    CloseHandle(hMap);
    CloseHandle(hEvent);
    return 0;
}
