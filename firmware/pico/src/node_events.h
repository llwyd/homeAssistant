#ifndef NODE_EVENTS_H_
#define NODE_EVENTS_H_

#include "state.h"

#define SIGNALS(SIG ) \
    SIG( Tick ) \
    SIG( ReadSensor ) \
    SIG( WifiCheckStatus ) \
    SIG( WifiConnected ) \
    SIG( WifiDisconnected ) \
    SIG( TCPRetryConnect ) \
    SIG( TCPConnected ) \
    SIG( TCPDisconnected ) \
    SIG( MessageReceived ) \
    SIG( MQTTRetryConnect ) \
    SIG( AccelDataReady ) \
    SIG( AccelMotion ) \
    SIG( DNSReceived ) \
    SIG( DNSRetryRequest ) \
    SIG( NTPReceived ) \
    SIG( NTPRetryRequest ) \
    SIG( AlarmElapsed ) \
    SIG( HandleCommand ) \

GENERATE_SIGNALS( SIGNALS );
GENERATE_SIGNAL_STRINGS( SIGNALS );

#endif /* NODE_EVENTS_H_ */
