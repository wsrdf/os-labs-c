// Клиент: отправляет серверу сигнал, затем числа и команды через канал.
// Сборка: gcc lab4_client.c -o lab4_client.exe
#include <stdio.h>
#include <stdlib.h>
#include "lab4.h"

static HANDLE pipe = INVALID_HANDLE_VALUE;

static int send_msg(LONG cmd, LONG value) {
    Lab4Message msg = { cmd, value };
    DWORD written;
    if (!WriteFile(pipe, &msg, sizeof msg, &written, NULL)) {
        fprintf(stderr, "Ошибка записи в канал: %lu\n", GetLastError());
        return 0;
    }
    return 1;
}

static int connect_to_server(void) {
    HANDLE ev = OpenEventA(EVENT_MODIFY_STATE, FALSE, LAB4_EVENT_NAME);
    if (!ev) {
        fprintf(stderr, "Сервер не запущен (OpenEvent: %lu)\n", GetLastError());
        return 0;
    }
    SetEvent(ev); // сигнал: создать канал
    CloseHandle(ev);

    for (int i = 0; i < 50; i++) { // сервер создаёт канал не мгновенно
        pipe = CreateFileA(LAB4_PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (pipe != INVALID_HANDLE_VALUE)
            break;
        Sleep(100);
    }
    if (pipe == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Не удалось открыть канал: %lu\n", GetLastError());
        return 0;
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
    printf("Подключено к серверу\n");
    return 1;
}

static void request_data(void) {
    if (!send_msg(CMD_GET, 0))
        return;

    LONG n;
    DWORD got;
    if (!ReadFile(pipe, &n, sizeof n, &got, NULL) || got != sizeof n) {
        fprintf(stderr, "Ошибка чтения ответа: %lu\n", GetLastError());
        return;
    }
    printf("Данные от сервера (%ld, по возрастанию):", (long)n);
    if (n > 0) {
        LONG *buf = (LONG *)malloc(sizeof(LONG) * n);
        if (buf && ReadFile(pipe, buf, sizeof(LONG) * n, &got, NULL) && got == sizeof(LONG) * n) {
            for (LONG i = 0; i < n; i++)
                printf(" %ld", (long)buf[i]);
        } else {
            fprintf(stderr, "\nОшибка чтения данных: %lu", GetLastError());
        }
        free(buf);
    }
    printf("\n");
}

static void disconnect(LONG cmd) {
    if (pipe != INVALID_HANDLE_VALUE) {
        send_msg(cmd, 0);
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }
}

int main(void) {
    SetConsoleOutputCP(CP_UTF8);

    for (;;) {
        printf("\n1 - сигнал серверу и подключение к каналу\n"
               "2 - отправить число\n"
               "3 - команда серверу: вывести дерево на его экран\n"
               "4 - получить данные от сервера\n"
               "5 - закрыть сеанс\n"
               "0 - завершить сервер и выйти\n> ");
        fflush(stdout);

        int choice;
        if (scanf("%d", &choice) != 1)
            break;

        if (choice == 1) {
            if (pipe != INVALID_HANDLE_VALUE)
                printf("Уже подключено\n");
            else
                connect_to_server();
        } else if (choice >= 2 && choice <= 5 && pipe == INVALID_HANDLE_VALUE) {
            printf("Сначала подключитесь (пункт 1)\n");
        } else if (choice == 2) {
            long value;
            printf("Число: ");
            fflush(stdout);
            if (scanf("%ld", &value) != 1)
                break;
            if (send_msg(CMD_INSERT, (LONG)value))
                printf("Отправлено: %ld\n", value);
        } else if (choice == 3) {
            send_msg(CMD_PRINT, 0);
        } else if (choice == 4) {
            request_data();
        } else if (choice == 5) {
            disconnect(CMD_CLOSE);
        } else if (choice == 0) {
            disconnect(CMD_EXIT);
            break;
        }
    }

    disconnect(CMD_CLOSE);
    return 0;
}
