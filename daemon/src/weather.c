#include "weather.h"
#include "comms.h"
#include "msg_fifo.h"
#include "json.h"

#define SOCKET_BUFFER_LEN (2048U)
#define API_PATH_LEN (128U)
#define JSON_PACKET_SIZE (64U)
#define ABSOLUTE_ZERO (-273.15f)

static comms_t comms;
static msg_fifo_t msg_fifo;
static char * api_key;
static char * location;
static char * url = "api.openweathermap.org";
static char * port = "80";

static uint8_t send_buffer[SOCKET_BUFFER_LEN];
static uint8_t recv_buffer[SOCKET_BUFFER_LEN];
static char json_packet[JSON_PACKET_SIZE];

static float current_temp = 0.0f;
static float current_hum = 0.0f;
static bool measurement_valid = false;

static void ExtractAndPrint(uint8_t * const buffer, char * keyword)
{
    assert(buffer != NULL);
    assert(keyword != NULL);

    char text[32];
    memset(text,0x00,32);
    JSON_LazyExtract(recv_buffer, text, keyword);
    printf("%s: %s\n",keyword, text);
}

static float ExtractFloat(uint8_t * const buffer, char * keyword)
{
    assert(buffer != NULL);
    assert(keyword != NULL);

    char text[32];
    float ret = 0.0f;
    memset(text,0x00,32);
    JSON_LazyExtract(recv_buffer, text, keyword);
    printf("%s: %s\n",keyword, text);

    ret = atof(text);
    return ret;
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
            ExtractAndPrint(recv_buffer, "description");
            current_temp = ExtractFloat(recv_buffer, "temp");
            current_temp += ABSOLUTE_ZERO;
            current_hum = ExtractFloat(recv_buffer, "humidity");
            measurement_valid = true;
            Comms_Disconnect(comms);
        }
        else
        {
            printf("Failed to send\n");
            measurement_valid = false;
            Comms_Disconnect(comms);
        }
    }
    else
    {
        printf("Failed to connect\n");
        measurement_valid = false;
    }
    Comms_Disconnect(comms);
}

void Weather_Init( char * key, char * loc )
{
    printf("Initialising Weather API\n");
   
    api_key = key;
    location = loc;
    memset(&comms, 0x00, sizeof(comms_t));
    comms.ip = url;
    comms.port = port;
    comms.fifo = &msg_fifo;
    Message_Init(&msg_fifo);
    Comms_Init(&comms);
}

void Weather_Read( void )
{
    GetWeatherInfo(&comms);
}

extern float Weather_GetTemperature( void )
{
}

extern char * Weather_GenerateJSON(void)
{
    memset(json_packet, 0x00, JSON_PACKET_SIZE);

    snprintf(json_packet, JSON_PACKET_SIZE,
            "{\"temperature\":%.1f,\"humidity\":%.1f,\"uptime_ms\":%u}",
            current_temp,
            current_hum,
            0U);
    return json_packet;
}

extern bool Weather_DataValid(void)
{
    return measurement_valid;
}

