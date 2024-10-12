#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include "fifo_base.h"
#include "state.h"
#include "mqtt.h"
#include "sensor.h"
#include "timestamp.h"
#include "events.h"
#include "timer.h"
#include "comms.h"
#include "msg_fifo.h"
#include "meta.h"
#include "json.h"

#define SOCKET_BUFFER_LEN (2048U)
#define API_PATH_LEN (128U)

static comms_t comms;
static msg_fifo_t msg_fifo;
static char * api_key;
static char * location;
static char * url = "api.openweathermap.org";
static char * port = "80";

static uint8_t send_buffer[SOCKET_BUFFER_LEN];
static uint8_t recv_buffer[SOCKET_BUFFER_LEN];

static void ExtractAndPrint(uint8_t * const buffer, char * keyword)
{
    assert(buffer != NULL);
    assert(keyword != NULL);

    char text[32];
    memset(text,0x00,32);
    JSON_LazyExtract(recv_buffer, text, keyword);
    printf("%s: %s\n",keyword, text);
}

static void GetWeatherInfo(comms_t * const comms)
{
    char api_path[API_PATH_LEN];
    memset(api_path, 0x00, API_PATH_LEN);
    snprintf(api_path, API_PATH_LEN,"/data/2.5/weather?q=%s&appid=%s",location, api_key);

    memset(send_buffer, 0x00, SOCKET_BUFFER_LEN);
    memset(recv_buffer, 0x00, SOCKET_BUFFER_LEN);
    int req_len = snprintf(send_buffer,
                SOCKET_BUFFER_LEN,
                "GET %s HTTP/1.1\r\nHost: %s:%s\r\nAccept: */*\r\nUser-Agent: pi\r\n\r\n",api_path , comms->ip ,comms->port);

    printf("%s\n", send_buffer);

    if(Comms_Connect(comms))
    {
        /* Connection established */
        if(Comms_Send(comms, send_buffer, req_len))
        {
            (void)Comms_Recv(comms, recv_buffer, SOCKET_BUFFER_LEN);
            printf("\n%s\n\n", recv_buffer);
            ExtractAndPrint(recv_buffer, "description");
            ExtractAndPrint(recv_buffer, "temp"); 
            ExtractAndPrint(recv_buffer, "humidity"); 
            Comms_Disconnect(comms);
        }
        else
        {
            printf("Failed to send\n");
            Comms_Disconnect(comms);
        }
    }
    else
    {
        printf("Failed to connect\n");
    }
}

bool Init( int argc, char ** argv )
{
    bool success = false;
    bool key_found = false;
    bool loc_found = false;
    int input_flags;

    memset(&comms, 0x00, sizeof(comms_t));
    comms.ip = url;
    comms.port = port;
    comms.fifo = &msg_fifo;


    while( ( input_flags = getopt( argc, argv, "k:l:" ) ) != -1 )
    {
        switch( input_flags )
        {
            case 'k':
                api_key = optarg;
                key_found = true;
                break;
            case 'l':
                location = optarg;
                loc_found = true;
            default:
                break;
        }
    } 
   
    success = key_found && loc_found;

    if(success)
    {
        printf("Location: %s\n", location);
        printf("Weather key: %s\n", api_key);
        Message_Init(&msg_fifo);
        Comms_Init(&comms);
        GetWeatherInfo(&comms);
    }

    return success;
}

int main( int argc, char ** argv )
{
    bool success = Init(argc, argv);
    if( success )
    {
        
    }
    else
    {
        printf("[CRITICAL ERROR!] FAILED TO INITIALISE\n");
    }
    return 0U;
}
    

