#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <cmath>
#include <stack>
#include <unistd.h>

#include <net/if.h>
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/ioctl.h>
#include <sys/socket.h>

#define ARPHRD_RAWIP    519 /* Raw IP  */

struct properties_t 
{
    unsigned channel;           ///< идентификатор канала
    unsigned char source[6];    ///< mac адрес отправителя
    unsigned repeat_send;       ///< период генерации кадров
    unsigned repeat_report;     ///< период генерации файлов с результатами вычислений
    char *folder;               ///< каталог для файлов с результатами вычислений
};  ///< значения командной строки

class frame_factory_t
{
    public:
    frame_factory_t();
    ~frame_factory_t(){}
    
    void frame_set_destination( unsigned char* );
    void frame_set_source( unsigned char* );
    void frame_set_ethertype( char, char );
    
    void frame_set_1dm_pdu_meglevel( properties_t* );
    void frame_set_1dm_pdu_opcode( char );
    void frame_set_1dm_pdu_flags( char );
    void frame_set_1dm_pdu_tlv_offset( char );
    void frame_set_1dm_pdu_tx();
    void frame_set_1dm_pdu_reserved();
    void frame_set_1dm_pdu_end_tlv( char );

    unsigned frame_get_1dm_pdu_meglevel();
    unsigned frame_get_1dm_pdu_tx_stamp_s();
    unsigned frame_get_1dm_pdu_tx_stamp_n();
    
    struct frame_t
    {   
        unsigned char destination[6];
        unsigned char source[6];
        unsigned char ethertype[2];
    
        unsigned char _1dm_pdu_meglevel[1];
        unsigned char _1dm_pdu_opcode;
        unsigned char _1dm_pdu_flags;
        unsigned char _1dm_pdu_tlv_offset;
        unsigned char _1dm_pdu_tx_stamp_s[4];
        unsigned char _1dm_pdu_tx_stamp_n[4];
        unsigned char _1dm_pdu_reserved[8];
        unsigned char _1dm_pdu_end_tlv;
    } frame;
    
    private:
    struct timespec sendstamp;
};

frame_factory_t::frame_factory_t()
{
    frame_set_ethertype( 0x89, 0x1F );

    frame_set_1dm_pdu_opcode( 45 );
    frame_set_1dm_pdu_flags( 0 );
    frame_set_1dm_pdu_tlv_offset( 16 );
    frame_set_1dm_pdu_reserved();
    frame_set_1dm_pdu_end_tlv( 0 );
}

class sender_t
{
    public:
    sender_t(){}
    ~sender_t(){}
    
    void start( unsigned char*, properties_t* );
    void stop(){ std::cout << "undefinded handler" << std::endl; }
};

class recipient_t
{
    public:
    recipient_t(){}
    ~recipient_t(){}
    
    void start( properties_t* );
    void stop(){ std::cout << "undefinded handler" << std::endl; }
    
    private:
    // void parse_n_save( frame_factory_t );
    // void write_journal();
    struct timespec recvstamp;
    struct timeval repeat_report;
    struct network_journal_t
    {
        unsigned tx_s;
        unsigned tx_ns;
        unsigned meglevel;        
        long double difference;
    };
};

int getIfx( unsigned char *source )   
{
    struct if_nameindex *ifs;
    struct ifreq ifr;
    
    ifs = if_nameindex();
    ifr.ifr_addr.sa_family = AF_INET;
    int some_desc = socket( AF_INET, SOCK_DGRAM, 0 );

    for( unsigned scroll = 0; ifs[scroll].if_name != NULL; scroll++  )
    {    
        strncpy(( char* )ifr.ifr_name , (const char* )ifs[scroll].if_name, IFNAMSIZ );
        ioctl( some_desc, SIOCGIFHWADDR, &ifr );

        for( unsigned a = 0, equ = 0; a < 6; a++ )
        {   
            if( ifr.ifr_hwaddr.sa_data[a] == ( char )source[a])
                equ++;
            if( equ == 6 )
            {    
                close( some_desc );
                return ifs[scroll].if_index;
            }
        }
        memset( &ifr, 0, sizeof( ifr ));  
    }

    std::cout << "network interface not found, check source_mac: " << std::hex << source << std::endl;
    return 0;
}
void frame_factory_t::frame_set_destination( unsigned char *destination )
{
    std::memcpy( frame.destination, ( unsigned char* )destination, 6 );
}
void frame_factory_t::frame_set_source( unsigned char *source )
{
    std::memcpy( frame.source, ( unsigned char* )source, 6 );
}
void frame_factory_t::frame_set_ethertype( char hbyte, char lbyte )
{
    frame.ethertype[0] = hbyte;
    frame.ethertype[1] = lbyte;
}
void frame_factory_t::frame_set_1dm_pdu_tx()
{
    clock_gettime( CLOCK_REALTIME, &sendstamp );
    std::memcpy( frame._1dm_pdu_tx_stamp_s, &sendstamp.tv_sec, 4 );
    std::memcpy( frame._1dm_pdu_tx_stamp_n, &sendstamp.tv_nsec, 4 );
}
void frame_factory_t::frame_set_1dm_pdu_meglevel( properties_t *properties )
{
    unsigned meglevel( 7 &properties->channel );
    meglevel = meglevel * pow( 2, 5 );
    
    std::memcpy( frame._1dm_pdu_meglevel, &meglevel, 1 );
}
void frame_factory_t::frame_set_1dm_pdu_opcode( char opcode )
{
    frame._1dm_pdu_opcode = ( unsigned char )opcode;
}
void frame_factory_t::frame_set_1dm_pdu_flags( char flags )
{
    frame._1dm_pdu_flags = ( unsigned char )flags;
}
void frame_factory_t::frame_set_1dm_pdu_tlv_offset( char tlv_offset )
{
    frame._1dm_pdu_tlv_offset = ( unsigned char )tlv_offset;
}
void frame_factory_t::frame_set_1dm_pdu_reserved()
{
    std::memset( frame._1dm_pdu_reserved, 0, 8 );
}
void frame_factory_t::frame_set_1dm_pdu_end_tlv( char end_tlv )
{
    frame._1dm_pdu_end_tlv = ( unsigned char )end_tlv;
}
unsigned frame_factory_t::frame_get_1dm_pdu_meglevel()
{
    unsigned meglevel = 0;
    std::memcpy( &meglevel, ( const void* )frame._1dm_pdu_meglevel, 1 );

    return meglevel * pow( 2, -5 );
}
unsigned frame_factory_t::frame_get_1dm_pdu_tx_stamp_s()
{
    unsigned tx_stamp_s = 0;
    std::memcpy( &tx_stamp_s, ( const void* )frame._1dm_pdu_tx_stamp_s, 4 );
    
    return tx_stamp_s;
}
unsigned frame_factory_t::frame_get_1dm_pdu_tx_stamp_n()
{
    unsigned tx_stamp_n = 0;
    std::memcpy( &tx_stamp_n, ( const void* )frame._1dm_pdu_tx_stamp_n, 4 );
    
    return tx_stamp_n;
}

void recipient_t::start( properties_t *properties )
{
    frame_factory_t frame_factory;
    network_journal_t* network_journal;
    std::stack< network_journal_t* > data;
    
    struct sockaddr_ll z;
    
    int packet_socket = socket( PF_PACKET, SOCK_RAW, IPPROTO_RAW );
    if( packet_socket < 0 )
        std::cout << "socket: " << strerror( errno ) << std::endl;
    
    z.sll_family = AF_PACKET;
    z.sll_protocol = htons( 0x891F );
    z.sll_ifindex = getIfx( properties->source );
    z.sll_pkttype = PACKET_HOST;
    
    if( bind( packet_socket, ( struct sockaddr* )&z, sizeof( z )) < 0 )
        std::cout << "bind: " << strerror( errno ) << std::endl;
    
    while( true )
    {
        repeat_report.tv_sec = properties->repeat_report;
        repeat_report.tv_usec = 0; 
        
        while( true )
        {
            fd_set connections;
            FD_ZERO( &connections );
            FD_SET( packet_socket, &connections );
            
            if( select( packet_socket + 1, &connections, NULL, NULL, &repeat_report ) < 0 )
                 std::cout << "select: " << strerror( errno ) << std::endl;

            clock_gettime( CLOCK_REALTIME, &recvstamp );
            
            if( repeat_report.tv_sec == 0 )
            {   // write report     void recipient_t::write_journal();

                std::string fullpath( properties->folder );
                std::string ext( "/T-REC-Y1731" );
                fullpath += ext;

                std::cout << "write " << fullpath << "\n";
                std::ofstream swap( fullpath, std::ofstream::binary );

                while( !data.empty())
                {
                    network_journal = data.top();
                    swap 
                        << network_journal->tx_s << "," << network_journal->tx_ns << "\t" 
                        << network_journal->meglevel << "\t" << network_journal->difference << std::endl;

                    data.pop();
                }
                swap.flush();
                swap.close();

                break;
            }
            
            if( recvfrom( packet_socket, ( void* )&frame_factory.frame, sizeof( frame_factory_t::frame_t ), 0, NULL, 0 ) < 0 )
                std::cout << "recvfrom: " << strerror( errno ) << std::endl;
            else
            {   // parse 'n save frame  void recipient_t::parse_n_save( frame_factory );

                network_journal = new network_journal_t;
                
                network_journal->meglevel = frame_factory.frame_get_1dm_pdu_meglevel();
                network_journal->tx_s = frame_factory.frame_get_1dm_pdu_tx_stamp_s();
                network_journal->tx_ns = frame_factory.frame_get_1dm_pdu_tx_stamp_n();
                network_journal->difference = ( double )( recvstamp.tv_sec - network_journal->tx_s ) 
                    + ( double )( recvstamp.tv_nsec - network_journal->tx_ns ) * ( double )pow( 10, -9 );
                
                std::cout << "network_journal->difference: " << network_journal->difference << "\n";
                
                data.push( network_journal );
            }
        }
    }
}
void sender_t::start( unsigned char *destination, properties_t *properties )
{
    struct sockaddr_ll x;
    frame_factory_t frame_factory;
    
    int packet_socket = socket( PF_PACKET, SOCK_RAW, IPPROTO_RAW );
    if( packet_socket < 0 )
        std::cout << "socket: " << strerror( errno ) << std::endl;
    
    x.sll_family = AF_PACKET;
    x.sll_ifindex = getIfx( properties->source );
    x.sll_hatype = ARPHRD_RAWIP;
    x.sll_pkttype = PACKET_OUTGOING;
    x.sll_halen = 6;    
    
    frame_factory.frame_set_destination( destination );
    frame_factory.frame_set_source( properties->source );
    frame_factory.frame_set_1dm_pdu_meglevel( properties );
    
    while( true )
    {   
        frame_factory.frame_set_1dm_pdu_tx();
        if( sendto( packet_socket, ( const void* )&frame_factory.frame, 
            sizeof( frame_factory_t::frame_t ), 0, ( struct sockaddr* )&x, sizeof( struct sockaddr_ll )) < 0 )
            std::cout << "sendto: " << strerror( errno ) << std::endl;
        
        sleep( properties->repeat_send );
    }    
}
void sender( unsigned char *destination, properties_t *properties )
{
    sender_t client;
    client.start( destination, properties );
}

int main( int argc, char **argv )
{
    properties_t properties;
    properties.channel = atoi( argv[1]);
    
    properties.repeat_send = atoi( argv[4 + argc - 7]);
    properties.repeat_report = atoi( argv[5 + argc - 7]);
    properties.folder = argv[6 + argc - 7];
    
    char *byte;
    byte = strtok( argv[2], ".-:" );
    for( unsigned hscroll = 0; byte != NULL; hscroll++ )
    {
        properties.source[hscroll] = strtol(( const char* )byte, NULL, 16 );
        byte = strtok( NULL, ".-:" );
    }

    unsigned char *destination;
    unsigned int destination_counter = argc - 6;
    if( argc >= 7 ) // если указан один или более destination mac, выполняется код отправителя
    {
        while( destination_counter )
        {
            destination = new unsigned char[6];
            // byte  = strtok( argv[3 + argc - 7], ".-:" );
            byte  = strtok( argv[destination_counter + 2], ".-:" );
            
            for( unsigned hscroll = 0; byte != NULL; hscroll++ )
            {
                destination[hscroll] = strtol(( const char* )byte, NULL, 16 );
                byte = strtok( NULL, ".-:" );
            }
        
            std::thread creative( sender, destination, &properties );
            creative.detach();
            
            destination_counter--;
        }
    }

    recipient_t server;
    server.start( &properties );
    
    return 0;
}
