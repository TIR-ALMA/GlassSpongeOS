#ifndef TCP_H
#define TCP_H

#include "../types.h"
#include "checksum.h"

#define TCP_HEADER_MIN_LEN 20
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20
#define TCP_FLAG_ECE 0x40
#define TCP_FLAG_CWR 0x80

struct tcp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset_reserved;
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed));

struct tcp_pseudo_header {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  reserved;
    uint8_t  protocol;
    uint16_t tcp_length;
} __attribute__((packed));

// TCP states
typedef enum {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSING,
    TCP_TIME_WAIT
} tcp_state_t;

// Socket types
typedef enum {
    TCP_SOCKET_PASSIVE,  // For listening
    TCP_SOCKET_ACTIVE    // For connecting
} tcp_socket_type_t;

// Buffer structures
struct tcp_buffer {
    uint8_t* data;
    size_t size;
    size_t head;
    size_t tail;
    size_t used;
};

// Outstanding segments table
struct tcp_outstanding_segment {
    uint32_t seq_num;
    void* data;
    size_t len;
    uint32_t sent_time;
    uint8_t retransmitted;
    struct tcp_outstanding_segment* next;
};

// TCP Socket structure
struct tcp_socket {
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t local_ip;
    uint32_t remote_ip;
    tcp_state_t state;
    tcp_socket_type_t type;
    
    // Sequence numbers
    uint32_t send_seq;      // Next segment to send
    uint32_t recv_seq;      // Next expected segment
    uint32_t send_ack;      // Last acknowledged by remote
    uint32_t recv_ack;      // Last ACK sent to remote
    
    // Window
    uint16_t advertised_window;  // Window announced by remote
    uint16_t our_window;         // Our window
    
    // Buffers
    struct tcp_buffer* send_buf;
    struct tcp_buffer* recv_buf;
    
    // Outstanding segments table
    struct tcp_outstanding_segment* outstanding_segments;
    
    // Timers
    uint32_t rtt;           // Round Trip Time
    uint32_t rto;           // Retransmission Timeout
    uint32_t last_ack_time; // Time of last ACK
    
    // Connection parameters
    uint32_t mss;           // Maximum Segment Size
    uint8_t connected;      // Connection flag
    
    // For listening sockets
    struct tcp_socket* accept_queue[10];
    int accept_queue_head;
    int accept_queue_tail;
    int accept_queue_count;
    
    // Next socket in global list
    struct tcp_socket* next;
};

// Global variables
extern struct tcp_socket* tcp_sockets;

// Initialization functions
void tcp_init(void);

// Socket functions
int tcp_socket(int domain, int type, int protocol);
int tcp_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int tcp_listen(int sockfd, int backlog);
int tcp_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int tcp_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int tcp_send(int sockfd, const void* buf, size_t len, int flags);
int tcp_recv(int sockfd, void* buf, size_t len, int flags);
int tcp_close(int sockfd);

// Packet handling
void tcp_handle_packet(void* ip_payload, size_t payload_len, uint32_t src_ip, uint32_t dst_ip);

// Internal functions
uint16_t tcp_checksum(struct tcp_header* tcph, size_t tcp_len, uint32_t src_ip, uint32_t dst_ip);
int tcp_send_segment(struct tcp_socket* sock, uint8_t flags, const void* data, size_t len);
int tcp_enqueue_segment(struct tcp_socket* sock, uint32_t seq_num, const void* data, size_t len);
int tcp_dequeue_segment(struct tcp_socket* sock, uint32_t ack_num);
void tcp_retransmit_segments(struct tcp_socket* sock);
void tcp_cleanup_socket(struct tcp_socket* sock);

#endif

