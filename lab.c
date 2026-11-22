#include <windows.h>
#include <stdio.h>
#include <locale.h>

#define VK_ESCAPE_CODE 0x1B

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    
    HANDLE stdInput = GetStdHandle(STD_INPUT_HANDLE); // stdInput - обработчик ввода с клавиатуры (указатель на дескриптор)
    if (stdInput == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "GetStdHandle failed: %lu\n", GetLastError());
        return 1;
    }
    
    printf("Нажмите клавишу на клавиатуре от 0 до 9 (Esc - выход)\n");

    int first = -1;
    INPUT_RECORD record; // инициализация структуры для хранения нажатой клавиши
    DWORD count; // без него не работает

    while (ReadConsoleInputW(stdInput, &record, 1, &count)) {
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) // нам интересны только нажатия клавиш
            continue;

        KEY_EVENT_RECORD *key = &record.Event.KeyEvent;

        if (key->wVirtualKeyCode == VK_ESCAPE_CODE)
            break;

        WCHAR ch = key->uChar.UnicodeChar;
        if (ch < L'0' || ch > L'9')
            continue;

        int digit = ch - L'0';   // преобразование в int из WCHAR

        if (first < 0) {
            first = digit;
            printf("Первая цифра: %d\n", digit);
        } else {
            printf("Вторая цифра: %d\n", digit);
            printf("%d + %d = %d\n", first, digit, first + digit);
            first = -1;
        }
    }

    return 0;
}