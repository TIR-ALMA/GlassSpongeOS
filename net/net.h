#ifndef NETWORK_H
#define NETWORK_H

#include "types.h"
#include "lib/string.h"
#include "mm.h"

 --- Ethernet ---
#define ETH_PROTO_ARP   0x0806
#define ETH_PROTO_IP    0x0800
#define ETH_PROTO_IPV6  0x86DD
 ETH_ADDR_LEN    6

struct ethernet_frame {
    uint8_t dest_mac[ETH_ADDR_LEN];
    uint8_t src_mac[ETH_ADDR_LEN];
    uint16_t ethertype;
    uint8_t payload[1500]; // MTU = 1500
    size_t payload_len;
};

// --- IP ---
#define IP_4    4
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

struct ip_header {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
} __attribute__((packed));

// --- ARP ---
#define ARP_HW_ETH  1
#define ARP_PROTO_IP 0x0800
#define ARP_REQUEST 1
#define ARP_REPLY   2

struct arp_header {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t op;
    uint8_t  sha[6];
    uint8_t  spa[4];
    uint8_t  tha[6];
    uint8_t  tpa[4];
} __attribute__((packed));

// --- ICMP ---
#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8

struct icmp_header {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed));

// --- TCP ---
#define TCP_HEADER_MIN_LEN 20
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

struct tcp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint2_t seq;
    uint32_t ack;
    uint8_t  data_off;
    uint8_t  flags;
    uint16_t window;
    uint16_t check;
    uint16_t urg_ptr;
} __attribute__((packed));

// --- UDP ---
struct udp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t check;
} __attribute__((packed));

// --- Socket API ---
typedef enum {
    SOCK_STREAM = 1,
    SOCK_DGRAM  = 2
} sock_type_t;

typedef enum {
    TCP_STATE_CLOSED,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RCVD,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,
    TCP_STATE_CLOSING,
    TCP_STATE_TIME_WAIT
} tcp_state_t;

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t  sin_zero[8];
};

struct socket {
    int;
    sock_type_t type;
    uint16_t local_port;
    uint32_t local_ip;
    uint16_t remote_port;
    uint32_t remote_ip;
    tcp_state_t state;
    
    // For TCP
    uint32_t snd_nxt;     // next send seq
    uint32_t rcv_nxt;     // next recv seq
    uint16_t snd_wnd;     // send window
    uint16_t rcv_wnd;     // recv window
    
    // Buffering
    uint8_t* recv_buf;
    size_t recv_buf_size;
    size_t recv_buf_used;
    size_t recv_buf_read;
    
    // For UDP
    struct socket* next_udp;
    
    // Linked list
    struct socket* next;
};

// Global state
extern struct socket* socket_list;
extern uint32_t net_local_ip;
extern uint8_t net_local_mac[6];

void network_init(void);
void network_handle_frame(struct ethernet_frame* frame);
void network_poll(void); // called from timer interrupt
uint32_t network_get_ip(void);
uint8_t* network_get_mac(void);

#endif
