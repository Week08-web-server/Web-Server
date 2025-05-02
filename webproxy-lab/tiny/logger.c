// logger.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "logger.h"

void log_message(const char *message)
{
    FILE *logFile = fopen("log.txt", "a");
    if (logFile == NULL)
    {
        perror("log.txt 열기 실패");
        return;
    }

    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = 0; // '\n' 제거

    fprintf(logFile, "[%s] %s\n", timestamp, message);

    fclose(logFile);
}
