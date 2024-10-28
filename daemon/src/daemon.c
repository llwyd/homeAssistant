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

#include "daemon_sm.h"
#include "daemon_events.h"
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

typedef struct
{
    daemon_settings_t daemon; 
    char * api_key;
    char * location;
    bool weather_enabled;
}
settings_t;

static void RefreshEvents( daemon_fifo_t * events )
{
    Daemon_RefreshEvents(events);
}

static void Loop( daemon_fifo_t * fifo )
{
    while( 1 )
    {
        /* Get Event */    
        while( FIFO_IsEmpty( (fifo_base_t *)fifo ) )
        {
            RefreshEvents( fifo );
        }

        state_event_t latest = FIFO_Dequeue( fifo );

        /* Dispatch */
        STATEMACHINE_FlatDispatch( latest.state, latest.event );
    }
}

bool HandleArgs( int argc, char ** argv, settings_t * settings )
{
    assert(settings != NULL);
    bool success = false;
    int input_flags;
    
    memset(settings, 0x00, sizeof(settings_t));

    while( ( input_flags = getopt( argc, argv, "b:c:k:l:" ) ) != -1 )
    {
        switch( input_flags )
        {
            case 'b':
                settings->daemon.broker_ip = optarg;
                settings->daemon.broker_port = "1883";
                break;
            case 'c':
                settings->daemon.client_name = optarg;
                break;
            case 'k':
                settings->api_key = optarg;
                break;
            case 'l':
                settings->location = optarg;
                break;
            default:
                break;
        }
    } 
   
    success =   (settings->daemon.broker_ip != NULL) &&
                (settings->daemon.broker_port != NULL) &&
                (settings->daemon.client_name != NULL);
   
    settings->weather_enabled = (settings->api_key != NULL) &&
                                (settings->location != NULL);

    return success;
}

static void WelcomeMessage(void)
{
    printf("!---------------------------!\n");
    printf("    Home Assistant Daemon\n\n");
    printf("    Git Hash: %s\n", META_GITHASH);
    printf("    Build Date: %s\n", META_DATESTAMP);
    printf("    Build Time: %s\n", META_TIMESTAMP);
    printf("!---------------------------!\n");
}

static void FailureMessage(void)
{
    printf("[CRITICAL ERROR!] FAILED TO INITIALISE\n");
    printf("Missing flag(s):\n");
    printf("\t-b\tMQTT broker IP\n");
    printf("\t-c\tClient Name\n");
    printf("\t-k\tAPI Key\n");
    printf("\t-l\tLocation\n");
}

int main( int argc, char ** argv )
{
    static settings_t settings;
    daemon_fifo_t event_fifo;
 
    WelcomeMessage();
    bool success = HandleArgs( argc, argv, &settings);
    DaemonEvents_Init(&event_fifo);
    (void)TimeStamp_Generate();
    Timer_Init();

    if( success )
    {
        Daemon_Init(&settings.daemon, &event_fifo);
        Loop(&event_fifo);
    }
    else
    {
        FailureMessage();
    }
    return 0U;
}

