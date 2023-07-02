#include "mqtt.h"

#include <time.h>
#include <assert.h>

#define MQTT_PORT ( "1883" )
#define BUFFER_SIZE (128)
#define ID_BUFFER_SIZE (32)

#define MQTT_TIMEOUT ( 0xb4 )

#define MQTT_CONNACK_CODE   ( 0x20 )
#define MQTT_SUBACK_CODE    ( 0x90 )

#define MQTT_PUBLISH_CODE   ( 0x30 )
#define MQTT_PUBACK_CODE    ( 0x40 )
#define MQTT_PINGRESP_CODE  ( 0xD0 )
#define MQTT_DISCONNECT_CODE  ( 0x00 )

typedef enum mqtt_msg_type_t
{
    mqtt_msg_Connect,
    mqtt_msg_Subscribe,
    mqtt_msg_Publish,
    mqtt_msg_Ping,
    mqtt_msg_Disconnect,
} mqtt_msg_type_t;


typedef struct mqtt_pub_t
{
    char * name;
    char * data;
} mqtt_pub_t;

typedef struct msg_id_t
{
    unsigned char read_index;
    unsigned char write_index;
    unsigned char fill;
    uint16_t id[ID_BUFFER_SIZE];
} msg_id_t;

/* Functions for handling acks */
bool Ack_Connect( uint8_t * buff, uint8_t len );
bool Ack_Subscribe( uint8_t * buff, uint8_t len );
bool Ack_Publish( uint8_t * buff, uint8_t len );
bool Ack_Ping( uint8_t * buff, uint8_t len );
bool Ack_Disconnect( uint8_t * buff, uint8_t len );

static void IncrementSendMessageID(void);
static bool CheckMsgIDBuffer( uint16_t val);
static void FlushMsgIDBuffer( void );

typedef struct mqtt_pairs_t
{
    mqtt_msg_type_t msg_type;
    uint8_t send_code;
    uint8_t recv_code;
    bool (*ack_fn)(uint8_t * buff, uint8_t len);
    char * name;
    char * ack_name;
} mqtt_pairs_t;

/* transmit and acknowledge code pairs */

static mqtt_pairs_t msg_code [ 5 ] =
{
    { mqtt_msg_Connect,         0x10, MQTT_CONNACK_CODE, Ack_Connect, "CONNECT", "CONNACK" },
    { mqtt_msg_Subscribe,       0x82, MQTT_SUBACK_CODE, Ack_Subscribe, "SUBSCRIBE", "SUBACK" },
    { mqtt_msg_Publish,         MQTT_PUBLISH_CODE, MQTT_PUBACK_CODE, Ack_Publish, "PUBLISH", "PUBACK" },
    { mqtt_msg_Ping,            0xc0, MQTT_PINGRESP_CODE, Ack_Ping, "PINGREQ", "PINGRESP" },
    { mqtt_msg_Disconnect,      0x00, MQTT_DISCONNECT_CODE, Ack_Disconnect, "DISCONNECT", "DISCONNECT" },
};

static int * sock;

static uint8_t send_buffer[ BUFFER_SIZE ];
static uint8_t recv_buffer[ BUFFER_SIZE ];

static char * client_name;
static char * parent_topic = "home";

static mqtt_subs_t * sub;
static uint8_t num_sub = 0;
static uint8_t successful_subs = 0;

static char * broker_ip;
static char * broker_port;

static uint16_t send_msg_id = 0x0000;

msg_id_t msg_id;

mqtt_data_t Extract( char * data, mqtt_type_t type )
{
    mqtt_data_t d;
    
    switch( type )
    {
        case mqtt_type_float:
            d.f = (float)atof(data);
            break;
        case mqtt_type_int16:
            d.i = (uint16_t)atoi(data);
            break;
        case mqtt_type_str:
            d.s = data;
            break;
        case mqtt_type_bool:
            if( strcmp("1", data) == 0) 
            {
                d.b = true; 
            }
            else
            {
                d.b = false;
            }
            break;
        default:
            assert(false);
            break;
    }
    return d;
}

bool Ack_Connect( uint8_t * buff, uint8_t len )
{
    bool ret = false;

    uint8_t message_code = *buff++;
    uint8_t msg_len = *buff++;

    (void)message_code;
    (void)msg_len;
    assert( msg_len == len );

    /* Reserved byte */
    buff++;
    
    if( *buff == 0x00 )
    {
        printf("\tCONNACK:OK\n");

        /* Need message queue flush here */
        send_msg_id = 0x1;
        successful_subs = 0x0;

        FlushMsgIDBuffer();
        ret = true;
    }
    else
    {
        printf("\tCONNACK:FAIL\n");
        ret = false;
    }

    return ret;
}

bool Ack_Subscribe( uint8_t * buff, uint8_t len )
{
    bool ret = false;
    uint8_t message_code = *buff++;
    uint8_t msg_len = *buff++;
    (void)message_code;
    (void)msg_len;

    assert( msg_len == len );
    
    uint16_t msg_id =  ( *buff++ << 8 );
    msg_id |= *buff++;

    printf("\tSUBACK msg id: %d\n", msg_id );

    bool success = CheckMsgIDBuffer( msg_id );

    if( success )
    {
        ret = true;
        successful_subs++;
        printf("\tSUBACK: OK\n");
    }
    else
    {
        ret = false;
        printf("\tSUBACK: FAIL\n");
        assert(false);
    }

    return ret;
}

bool Ack_Publish( uint8_t * buff, uint8_t len )
{
    uint8_t return_code = buff[0];
    uint8_t msg_len = buff[1];

    assert( msg_len == len );

    bool ret = false;
    uint16_t msg_id =  ( buff[2] << 8 ) | buff[3];


    if( return_code == MQTT_PUBACK_CODE )
    {
        printf("\tPUBACK msg id: %d\n", msg_id );
        bool success = CheckMsgIDBuffer( msg_id );

        if( success )
        {
            ret = true;
            printf("\tPUBACK: OK\n");
        }
        else
        {
            ret = false;
            printf("\tPUBACK: FAIL\n");
            assert(false);
        }
    }
    else
    {
        char topic[64];
        char full_topic[64];
        char data[32];

        memset( topic, 0x00, 64);
        memset( data, 0x00, 32);
                
        unsigned char msg_len = len;
        unsigned char topic_len = buff[2] << 8 | buff[3];
        unsigned char * msg = &buff[4];
        
        memcpy(topic, msg, topic_len);
        msg += topic_len;
        msg_len -= topic_len;
        printf("\tPUBLISH msg id: %d\n", msg_id );
        printf("\t topic: %s\n", topic );
        
        for( uint8_t i = 0; i < num_sub ; i++ )
        {
            memset(full_topic, 0x00, 64);
            
            strcat(full_topic, parent_topic);
            strcat(full_topic,"/");
            strcat(full_topic, sub[i].name);
            strcat(full_topic, "/");
            strcat(full_topic, client_name );
            
            if( strcmp( full_topic, topic) == 0)
            {
                memcpy(data, msg, msg_len);
                printf("\t  data:%s\n",data);
                mqtt_data_t decode_data = Extract( data, sub[i].format);
                sub[i].sub_fn( &decode_data );
                
                ret = true;
            }
        }

    }
    return ret;
}

bool Ack_Ping( uint8_t * buff, uint8_t len )
{
    (void)buff;
    (void)len;
    return true;
}

bool Ack_Disconnect( uint8_t * buff, uint8_t len )
{
    (void)buff;
    (void)len;
    return true;
}

bool MQTT_AllSubscribed( void )
{
    return ( num_sub == successful_subs );
}

static void FlushMsgIDBuffer( void )
{
    if( msg_id.fill > 0U )
    {
        msg_id.read_index = msg_id.write_index;
        msg_id.fill = 0U;
    }
}

static void AddMessageIDToQueue( uint16_t id )
{
    if( msg_id.fill < ID_BUFFER_SIZE )
    {
        msg_id.id[ msg_id.write_index++ ] = id;
        msg_id.fill++;
        msg_id.write_index = ( msg_id.write_index & ( ID_BUFFER_SIZE - 1U ) );
    }
    else
    {
        printf("\tMQTT Error! Ack buffer full\n");
        assert(false);
    }
}

static bool MessagesAvailable( void )
{
    bool available = (msg_id.fill > 0U );
    return available;
}

static uint16_t GetMessageIDFromQueue( void )
{
    uint16_t val = 0x0;
    if( msg_id.fill > 0U )
    {
        val = msg_id.id[ msg_id.read_index++ ];
        msg_id.fill--;
        msg_id.read_index = ( msg_id.read_index & ( ID_BUFFER_SIZE - 1U ) );
    }

    return val;
}

static void IncrementSendMessageID(void )
{
    send_msg_id++; 

    if( send_msg_id == 0U )
    {
        send_msg_id = 0x1;
    }
}

static bool CheckMsgIDBuffer( uint16_t id )
{
    bool found = false;
    
    if( MessagesAvailable() )
    {
        unsigned char fill_copy = msg_id.fill;

        for( int i = 0; i < fill_copy; i++ )
        {
            uint16_t val = GetMessageIDFromQueue();
            if( val == id )
            {
                found = true;
                break;
            }
            else
            {
                AddMessageIDToQueue( val );
            }
        }
    }
    else
    {
        printf("\tMQTT Error! No messages in buffer\n");
        assert(false);
    }

    return found;
}

static uint16_t Format( mqtt_msg_type_t msg_type, void * msg_data )
{
    uint16_t full_packet_size = 0;

    switch( msg_type )
    {
        case mqtt_msg_Connect:
        {
            /* Message Template for mqtt connect */
            unsigned char mqtt_template_connect[] =
            {
                0x10,                                   /* connect */
                0x00,                                   /* payload size */
                0x00, 0x06,                             /* Protocol name size */
                0x4d, 0x51, 0x49, 0x73, 0x64, 0x70,     /* MQIsdp */
                0x03,                                   /* Version MQTT v3.1 */
                0x02,                                   /* Fire and forget */
                0x00, MQTT_TIMEOUT,                     /* Keep alive timeout */
            };
            
            uint8_t * msg_ptr = send_buffer;
            uint16_t packet_size = (uint16_t)sizeof( mqtt_template_connect );
            uint16_t name_size = strlen( client_name );

            memcpy( msg_ptr, mqtt_template_connect, packet_size );
            msg_ptr+= packet_size;
            msg_ptr++;
            *msg_ptr++ = (uint8_t)(name_size & 0xFF);
            memcpy(msg_ptr, client_name, name_size);
                
            uint16_t total_packet_size = packet_size + name_size;
            send_buffer[1] = (uint8_t)(total_packet_size&0xFF);
            full_packet_size = total_packet_size + 2;
        }
            break;

        case mqtt_msg_Subscribe:
        {
            mqtt_subs_t * sub_data = (mqtt_subs_t *) msg_data;
            const unsigned char mqtt_template_subscribe[] =
            {
                0x82,                                                                   // message type, qos, no retain
                0x00,                                                                   // length of message
            };
    
            char full_topic[64];
            memset( full_topic, 0x00, 64);
            
            strcat(full_topic, parent_topic);
            strcat(full_topic, "/");
            strcat(full_topic, sub_data->name);
            strcat(full_topic,"/");
            strcat(full_topic, client_name);

            memset(send_buffer, 0x00, 128);

            /* Construct Packet */
            uint8_t * msg_ptr = send_buffer;
            uint16_t packet_size = (uint16_t)sizeof( mqtt_template_subscribe );
            uint16_t topic_size = strlen(full_topic );

            memcpy( msg_ptr, mqtt_template_subscribe, packet_size );
            msg_ptr+= packet_size;
            
            *msg_ptr++ = (uint8_t)((send_msg_id >> 8U)&0xFF);
            *msg_ptr++ = (uint8_t)(send_msg_id&0xFF);
            
            msg_ptr++;
            *msg_ptr++ = (uint8_t)(topic_size & 0xFF);
            memcpy(msg_ptr, full_topic, topic_size);
     
            uint16_t total_packet_size = packet_size + topic_size + 1 + 2;
            send_buffer[1] = (uint8_t)(total_packet_size&0xFF);
            full_packet_size = total_packet_size + 2;
            
            AddMessageIDToQueue( send_msg_id );
            IncrementSendMessageID();
        }
            break;
        case mqtt_msg_Publish:
        {
            printf("\tConstructing Packet\n");

            mqtt_pub_t * pub_data = (mqtt_pub_t *) msg_data;
            
            memset(send_buffer, 0x00, 128);
            
            char text_buff[64];
            memset( text_buff, 0x00, 64);
            
            strcat(text_buff, parent_topic);
            strcat(text_buff,"/");
            strcat(text_buff, pub_data->name);
            strcat(text_buff, "/");
            strcat(text_buff, client_name);
            uint16_t topic_size = strlen(text_buff);

            printf("\t topic: %s\n", text_buff);
            printf("\t  size: %d\n", topic_size);

            uint8_t * msg_ptr = send_buffer;
            *msg_ptr++ = ( msg_code[mqtt_msg_Publish].send_code | 0x2 );
            msg_ptr++; /* This is where message length would go */ 
            msg_ptr++; /* MSB topic length */
            *msg_ptr++ = (uint8_t)(topic_size & 0xFF);
            memcpy(msg_ptr, text_buff, topic_size);
            memset( text_buff, 0x00, 64);
            msg_ptr+=topic_size;

            /* Message ID */
            *msg_ptr++ = (uint8_t)((send_msg_id >> 8U)&0xFF);
            *msg_ptr++ = (uint8_t)(send_msg_id&0xFF);
            
            strcat( text_buff, pub_data->data );

            uint8_t data_len = strlen(text_buff);
            
            printf("\t  data: %s\n", text_buff);
            printf("\t  size: %d\n", data_len);

            memcpy(msg_ptr, text_buff, data_len);

            /* Topic len + topic + data + msg id */
            uint8_t total_packet_size = (uint8_t)topic_size + data_len + 2 + 2;
            send_buffer[1] = total_packet_size;
            /* header and size byte */
            full_packet_size = total_packet_size + 2;

            AddMessageIDToQueue( send_msg_id );
            IncrementSendMessageID();
        }
            break;
        default:
        {
            assert(false);
        }
            break;
    }
    return full_packet_size;
}

static bool Transmit( mqtt_msg_type_t msg_type, void * msg_data )
{
    memset(send_buffer, 0x00, BUFFER_SIZE);
   
    bool ret = false;
    /* Construct Data Packet */ 
    uint16_t packet_size = Format( msg_type, msg_data );

    /* Send */
    int snd = send( *sock, send_buffer, packet_size, 0);
    if( snd < 0 )
    {
        printf("\t Error Sending Data\n");
        ret = false;
    }
    else
    {
        printf("\t MQTT Transmitting %s packet\n", msg_code[(int)msg_type].name);
        ret = true;
    }

    return ret;
}

static bool Decode( uint8_t * buffer, uint16_t len )
{
    (void)len;
    bool ret = false;
    uint8_t return_code = ( buffer[0] & 0xF0 );
    uint8_t msg_length = buffer[1];
    mqtt_msg_type_t msg_type;

    switch( return_code )
    {
        case MQTT_CONNACK_CODE:
            msg_type = mqtt_msg_Connect;
            break;
        case MQTT_PUBLISH_CODE:
        case MQTT_PUBACK_CODE:
            msg_type = mqtt_msg_Publish;
            break;
        case MQTT_SUBACK_CODE:
            msg_type = mqtt_msg_Subscribe;
            break;
        default:
            printf("\tMQTT ERROR! Bad Receive Packet\n");
            assert( false );
            break;
    }
    
    printf("\tMQTT %s packet received, length: %d\n", msg_code[(int)msg_type ].name, msg_length );
    ret = msg_code[(int)msg_type].ack_fn( buffer, msg_length );

    return ret;
}

extern bool MQTT_EncodeAndPublish( char * name, mqtt_type_t format, void * data )
{
    mqtt_pub_t mqtt_data;
    char datastr[64];
    memset( datastr, 0x00, 64 );

    mqtt_data.name = name;
    mqtt_data.data = datastr;
    
    switch( format )
    {
        case mqtt_type_float:
            {
                float d = *((float *)data);
                snprintf(datastr, 64, "%.4f", d);
            }
            break;
        case mqtt_type_int16:
            {
                int d = *((int *)data);
                snprintf(datastr, 64, "%df", d);
            }
            break;
        case mqtt_type_str:
            {
                char * d = ((char *)data);
                strcat(datastr, d);
            }
            break;
        case mqtt_type_bool:
            {
                bool d = *(( bool * )data);
                if( d )
                {
                    strcat(datastr,"1");
                }
                else
                {
                    strcat(datastr,"0");
                }
            }
            break;
        default:
            assert(false);
            break;
    }

    printf("\tPublish Data Encoded (%s / %s)\n", mqtt_data.name, mqtt_data.data );

    return Transmit( mqtt_msg_Publish, &mqtt_data );
}

extern bool MQTT_Receive( void )
{
    bool ret = false;
    memset(recv_buffer, 0x00, BUFFER_SIZE);

    unsigned char raw_peek[2] = {0x00, 0x00};    
    int peek = recv( *sock, raw_peek, 2U, MSG_PEEK );

    if( peek <= 0 )
    {
        printf("\tMQTT Error Peeking Data\n");
        ret = false;
    }
    else
    {
        unsigned char bytes_to_read = raw_peek[1] + 2U;

        int rcv = recv( *sock, recv_buffer, bytes_to_read, 0U);
        if( rcv < 0 )
        {
            printf("\tMQTT Error Sending Data\n");
            ret = false;
        }
        else if( rcv == 0 )
        {
            printf("\tMQTT Connection Closed\n");
            ret = false;
        }
        else
        {
            printf("\tMQTT Data Received\n");
    
            ret = Decode( recv_buffer, rcv );
            
            /* Below is useful for debugging */
            /*    
            printf("\t");
            for( int i = 0; i < rcv; i++ )
            {
                printf("0x%x,", recv_buffer[i]);
            }
            printf("\b \n");
            */
        }
    }
    return ret;
}

extern bool MQTT_Subscribe( void )
{
    assert( num_sub > 0U );

    bool success = true;
    for( uint8_t i = 0; i < num_sub; i++ )
    {
        success &= Transmit( mqtt_msg_Subscribe, &sub[i] );
    }

    return success;
}

extern bool MQTT_Connect( void )
{
    bool ret = false;
    
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset( &hints, 0U, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    /* 1. Resolve port information */
    status = getaddrinfo( broker_ip, broker_port, &hints, &servinfo );
    if( status != 0U )
    {
        fprintf( stderr, "Comms Error: %s\n", gai_strerror( status ) );	
        ret = false;
    }
    else
    {   
        /* 2. Create Socket */
        *sock = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
        if( *sock < 0 )
        {
            printf("Error creating socket\n");
            ret = false;
        }
        else
        {
            /* 3. Attempt Connection */
            int c = connect( *sock, servinfo->ai_addr, servinfo->ai_addrlen);
            if( c < 0 )
            {
                printf("\tConnection Failed\n");
                ret = false;
            }
            else
            {
                /* 4. Send Connect MQTT Packet */ 
                ret = Transmit( mqtt_msg_Connect, NULL );
            }
        }
    } 
    return ret;
}

extern void MQTT_Init( char * ip, char * name, int *mqtt_sock, mqtt_subs_t * subscriptions, uint8_t number_subs )
{
    assert( ip != NULL );
    assert( mqtt_sock != NULL );
    assert( name != NULL );

    printf("\tMQTT Initialising\n");
    
    broker_ip = ip;
    broker_port = MQTT_PORT;
    client_name = name;
    sock = mqtt_sock;

    sub = subscriptions;
    num_sub = number_subs;

    memset( msg_id.id, 0x00, ID_BUFFER_SIZE * sizeof(uint16_t) );

    printf("\tMQTT Broker ip: %s, port: %s\n", broker_ip, broker_port);
    printf("\tMQTT Client name: %s\n", client_name );

    if( num_sub > 0U )
    {
        printf("\tMQTT Subscription topics:\n");
        for( uint8_t i = 0; i < num_sub; i++ )
        {
            printf("\t\t%s\n", sub[i].name);
        }
    }
}

