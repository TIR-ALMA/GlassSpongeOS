#ifndef SOCKET_H
#define SOCKET_H

#include "../types.h"

// Socket domains
#define AF_UNSPEC     0
#define AF_UNIX       1
#define AF_INET       2
#define AF_INET6      10
#define AF_PACKET     17

// Socket types
#define SOCK_STREAM   1
#define SOCK_DGRAM    2
#define SOCK_RAW      3
#define SOCK_RDM      4
#define SOCK_SEQPACKET 5

// Protocol families
#define PF_UNSPEC     AF_UNSPEC
#define PF_UNIX       AF_UNIX
#define PF_INET       AF_INET
#define PF_INET6      AF_INET6
#define PF_PACKET     AF_PACKET

// Protocol numbers
#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_RAW     255

// Address families
#define AF_INET         2

// Socket options levels
#define SOL_SOCKET      1
#define SO_REUSEADDR    2
#define SO_BROADCAST    6
#define SO_KEEPALIVE    9
#define SO_LINGER       13
#define SO_RCVBUF       8
#define SO_SNDBUF       7

// Socket operations
#define SHUT_RD         0
#define SHUT_WR         1
#define SHUT_RDWR       2

// Error codes
#define EAGAIN          11
#define EWOULDBLOCK     11
#define EINPROGRESS     1 EISCONN         106
#define ENOTCONN        107
#define E104
#define ETIMEDOUT       110
#define ECONNREFUSED    111
#define ENETUNREACH     101
#define EADDRINUSE      98
#define EADDRNOTAVAIL   99

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

struct in_addr {
    uint32_t s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family;
    uint16_t    sin_port;
    struct in_addr sin_addr;
    uint8_t     sin_zero[8];
};

struct linger {
    int l_onoff;
    int l_linger;
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct sockaddr_storage {
    sa_family_t ss_family;
    char        __ss_padding[128 - sizeof(sa_family_t)];
};

// Socket option structure
struct sockopt {
    int reuseaddr;
    int broadcast;
    int keepalive;
    int sndbuf;
    int rcvbuf;
    struct linger linger_opt;
    int nonblock;
};

// Socket states
typedef enum {
    SS_FREE = 0,
    SS_UNCONNECTED,
    SS_CONNECTING,
    SS_CONNECTED,
    SS_DISCONNECTING,
    SS_BOUND,
    SS_LISTENING
} socket_state_t;

// Socket types
typedef enum {
    SOCKET_TCP,
    SOCKET_UDP,
    SOCKET_RAW
} socket_type_t;

// Socket structure
typedef struct socket {
    int                 fd;
    socket_type_t       type;
    socket_state_t      state;
    int                 domain;
    int                 protocol;
    uint16_t            local_port;
    uint16_t            remote_port;
    uint32_t            local_ip;
    uint32_t            remote_ip;
      options;
    
    // TCP-specific fields
    uint32_t            send_seq;
    uint32_t            recv_seq;
    uint32_t            window_size;
    uint8_t*            send_buffer;
    uint8_t*            recv_buffer;
    size_t              send_buffer_size;
    size_t              recv_buffer_size;
    size_t              send_buffer_pos;
    size_t              recv_buffer_pos;
    size_t              send_buffer_used;
    size_t              recv_buffer_used;
    
    // Connection queue for listening sockets
    struct socket*      conn_queue[32]; // Fixed-size connection queue
    int                 conn_queue_front;
    int                 conn_queue_back;
    int                 conn_queue_size;
    
    // Flags
    int                 connected;
    int                 listening;
    int                 bound;
    int                 closed;
    
    // Synchronization
    int                 waiting_for_data;
    int                 waiting_for_space;
    
    // Next socket in global list
    struct socket*      next;
} socket_t;

// Function prototypes
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int send(int sockfd, const void* buf, size_t len, int flags);
int recv(int sockfd, void* buf, size_t len, int flags);
int sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen);
int recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen);
int shutdown(int sockfd, int how);
int closesocket(int sockfd);
int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen);
int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen);
int ioctlsocket(int sockfd, long cmd, unsigned long* argp);
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);

// Helper functions
socket_t* socket_find_by_fd(int fd);
int socket_allocate_fd(void);
void socket_free_fd(int fd);
int socket_validate_params(int domain, int type, int protocol);

#endif

