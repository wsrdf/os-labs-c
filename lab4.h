#ifndef LAB4_H
#define LAB4_H

#include <windows.h>

#define LAB4_PIPE_NAME   "\\\\.\\pipe\\Lab4Pipe"
#define LAB4_EVENT_NAME  "Local\\Lab4_CreatePipe" // сигнал серверу: создать канал

// Команды клиента, передаваемые через канал
enum {
    CMD_INSERT = 1, // value - число для добавления в дерево
    CMD_PRINT,      // сервер выводит дерево на свой экран
    CMD_GET,        // сервер присылает клиенту все числа по возрастанию
    CMD_CLOSE,      // клиент закрывает сеанс, сервер ждёт нового сигнала
    CMD_EXIT        // завершить сервер
};

typedef struct {
    LONG cmd;
    LONG value;
} Lab4Message;

// Ответ на CMD_GET: сообщение с LONG count, затем (если count > 0) сообщение с count значениями LONG

#endif
