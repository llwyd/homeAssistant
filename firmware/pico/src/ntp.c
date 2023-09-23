#include "ntp.h"
#include "lwip/pbuf.h"
#include "lwip/dns.h"
#include "node_events.h"
#include "emitter.h"
#include <time.h>
#include "alarm.h"

#define NTP_IP ("pool.ntp.org")
#define NTP_PORT (123U)
#define NTP2UNIX (((70U * 365U) + 17U) * 86400U)

static ip_addr_t ntp_addr;
static const char * url = NTP_IP;
static time_t unixtime = 0U;

static void DNSFound(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    ntp_addr = *ipaddr;
    Emitter_EmitEvent(EVENT(DNSReceived));
}

extern void NTP_Init( ntp_t * ntp )
{
    printf("Initialising NTP\n");
}

extern void NTP_RequestDNS( ntp_t * ntp )
{
    printf("\tRequesting IP for %s\n", url);
    dns_gethostbyname(url, &ntp_addr, DNSFound, NULL);
}

extern void NTP_PrintIP( ntp_t * ntp )
{
    printf("\tIP Found: %s\n",ipaddr_ntoa(&ntp_addr));
}

extern void NTP_Get( ntp_t * ntp )
{
    uint8_t send_buffer[48U];
    send_buffer[0] = 0x23;
    ntp->send(send_buffer, 48U, ntp_addr, NTP_PORT);
}

extern void NTP_Decode( uint8_t * buffer )
{
    uint32_t * ptr = (uint32_t *)buffer;
    uint32_t raw[12U];
    for(uint32_t idx = 0; idx < 12U; idx++)
    {
        raw[idx] = __builtin_bswap32(ptr[idx]);
    }

    uint32_t timestamp = raw[10];
    timestamp -= NTP2UNIX;
    unixtime = (time_t)timestamp;
    Alarm_SetClock(&unixtime);
}

