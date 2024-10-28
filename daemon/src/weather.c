#include "weather.h"

#define SIGNALS(SIG ) \
    SIG( Tick ) \
    SIG( MessageReceived ) \
    SIG( Disconnect ) \
    SIG( UpdateHomepage ) \

GENERATE_SIGNALS( SIGNALS );
GENERATE_SIGNAL_STRINGS( SIGNALS );

DEFINE_STATE(Connect);
DEFINE_STATE(MQTTConnect);
DEFINE_STATE(Subscribe);
DEFINE_STATE(Idle);

extern void Weather_Init(void)
{

}

