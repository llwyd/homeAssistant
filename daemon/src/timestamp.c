#include <stdio.h>
#include <string.h>
#include <time.h>

#define TIMESTAMP_LEN (32U)
static char buffer[TIMESTAMP_LEN] = {0};

extern char * TimeStamp_Generate(void)
{
    time_t raw_time;
    struct tm * time_info;

    memset(buffer, 0x00, TIMESTAMP_LEN);

    time(&raw_time);
    time_info = localtime(&raw_time);

    strftime(buffer, TIMESTAMP_LEN, "%T", time_info);

    return buffer;
}

extern void TimeStamp_Print(void)
{
    printf("[%s] ", TimeStamp_Generate());
}

