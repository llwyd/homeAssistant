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

static comms_t comms;
static msg_fifo_t msg_fifo;
static char * api_key;
static char * location;
static char * url = "api.openweathermap.org";
static char * port = "80";
static uint8_t getPath[128];

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
    

