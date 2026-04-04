#include "socket.h"
#include "tcp.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "mm.h"

// Global socket list
static socket_t* socket_list = NULL;
static int next_socket_fd = 3; // Start from 3 (0,1,2 reserved for stdin/stdout/stderr)

// Initialize socket subsystem
void socket_init(void) {
    socket_list = NULL;
    next_socket_fd = 3;
    printf("Socket subsystem initialized\n");
}

// Find socket by file descriptor
socket_t* socket_find_by_fd(int fd) {
    socket_t* sock = socket_list;
    while(sock) {
        if(sock->fd == fd) {
            return sock;
        }
        sock = sock->next;
    }
    return NULL;
}

// Allocate a new socket file descriptor
int socket_allocate_fd(void) {
    return next_socket_fd++;
}

// Free socket file descriptor (not used in this simple implementation)
void socket_free_fd(int fd) {
    // In a real implementation, you might want to maintain a free list
    // For now, just let next_socket_fd grow
}

// Validate socket parameters
int socket_validate_params(int domain, int type, int protocol) {
    if(domain != AF_INET) {
        return -1; // Only IPv4 supported
    }
    
    if(type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_RAW) {
        return -1; // Unsupported socket type
    }
    
    if(protocol != 0 && protocol != IPPROTO_TCP && protocol != IPPROTO_UDP && protocol != IPPROTO_RAW) {
        return -1; // Unsupported protocol
    }
    
    return 0;
}

// Create a socket
int socket(int domain, int type, int protocol) {
    // Validate parameters
    if(socket_validate_params(domain, type, protocol) < 0) {
        return -1;
    }
    
    // Allocate socket structure
    socket_t* sock = (socket_t*)kmalloc(sizeof(socket_t));
    if(!sock) {
        return -1;
    }
    
    // Initialize socket structure
    memset(sock, 0, sizeof(socket_t));
    
    sock->fd = socket_allocate_fd();
    sock->domain = domain;
    sock->type = (type == SOCK_STREAM) ? SOCKET_TCP : 
                 (type == SOCK_DGRAM) ? SOCKET_UDP : SOCKET_RAW;
    sock->protocol = protocol ? protocol : ((type == SOCK_STREAM) ? IPPROTO_TCP : IPPROTO_UDP);
    sock->state = SS_UNCONNECTED;
    sock->connected = 0;
    sock->listening = 0;
    sock->bound = 0;
    sock->closed = 0;
    
    // Initialize options
    sock->options.reuseaddr = 0;
    sock->options.broadcast = 0;
    sock->options.keepalive = 0;
    sock->options.sndbuf = 4096;
    sock->options.rcvbuf = 4096;
    sock->options.linger_opt.l_onoff = 0;
    sock->options.linger_opt.l_linger = 0;
    sock->options.nonblock = 0;
    
    // Allocate buffers based on type
    if(sock->type == SOCKET_TCP) {
        sock->send_buffer_size = sock->options.sndbuf;
        sock->recv_buffer_size = sock->options.rcvbuf;
        sock->send_buffer = (uint8_t*)kmalloc(sock->send_buffer_size);
        sock->recv_buffer = (uint8_t*)kmalloc(sock->recv_buffer_size);
        if(!sock->send_buffer || !sock->recv_buffer) {
            if(sock->send_buffer) kfree(sock->send_buffer);
            if(sock->recv_buffer) kfree(sock->recv_buffer);
            kfree(sock);
            return -1;
        }
        sock->send_buffer_pos = 0;
        sock->recv_buffer_pos = 0;
        sock->send_buffer_used = 0;
        sock->recv_buffer_used = 0;
    }
    
    // Add to global socket list
    sock->next = socket_list;
    socket_list = sock;
    
    printf("Socket created: fd=%d, type=%d, protocol=%d\n", sock->fd, sock->type, sock->protocol);
    return sock->fd;
}

// Bind socket to local address
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(addrlen < sizeof(struct sockaddr_in)) {
        return -1;
    }
    
    struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
    
    // Check if already bound
    if(sock->bound) {
        return -1; // Already bound
    }
    
    // Validate address family
    if(addr_in->sin_family != AF_INET) {
        return -1;
    }
    
    // Set local address
    sock->local_ip = addr_in->sin_addr.s_addr;
    sock->local_port = addr_in->sin_port;
    
    // If port is 0, assign a dynamic port
    if(sock->local_port == 0) {
        static uint16_t dynamic_port = 1024;
        sock->local_port = dynamic_port++;
    }
    
    sock->bound = 1;
    sock->state = SS_BOUND;
    
    printf("Socket %d bound to %x:%d\n", sockfd, sock->local_ip, sock->local_port);
    return 0;
}

// Connect socket to remote address
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(addrlen < sizeof(struct sockaddr_in)) {
        return -1;
    }
    
    struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
    
    // Validate address family
    if(addr_in->sin_family != AF_INET) {
        return -1;
    }
    
    // Check if already connected
    if(sock->connected) {
        return -1; // Already connected
    }
    
    // Set remote address
    sock->remote_ip = addr_in->sin_addr.s_addr;
    sock->remote_port = addr_in->sin_port;
    
    // If not bound, bind to default local address
    if(!sock->bound) {
        sock->local_ip = 0x0a000001; // 10.0.0.1 as example
        static uint16_t dynamic_port = 1024;
        sock->local_port = dynamic_port++;
        sock->bound = 1;
        sock->state = SS_BOUND;
    }
    
    // For TCP sockets, initiate connection
    if(sock->type == SOCKET_TCP) {
        sock->state = SS_CONNECTING;
        
        // Send SYN packet (this would call TCP layer)
        // In a real implementation, this would involve a complex handshake
        // For now, simulate immediate connection
        sock->state = SS_CONNECTED;
        sock->connected = 1;
        
        printf("TCP socket %d connected to %x:%d\n", sockfd, sock->remote_ip, sock->remote_port);
    } else if(sock->type == SOCKET_UDP) {
        // For UDP, just set remote address
        sock->state = SS_CONNECTED;
        sock->connected = 1;
        
        printf("UDP socket %d connected to %x:%d\n", sockfd, sock->remote_ip, sock->remote_port);
    }
    
    return 0;
}

// Listen for connections on socket
int listen(int sockfd, int backlog) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(sock->type != SOCKET_TCP) {
        return -1; // Only TCP sockets can listen
    }
    
    if(!sock->bound) {
        return -1; // Must be bound first
    }
    
    if(backlog <= 0) {
        backlog = 1;
    }
    
    if(backlog > 32) {
        backlog = 32; // Maximum supported backlog
    }
    
    sock->listening = 1;
    sock->state = SS_LISTENING;
    
    // Initialize connection queue
    for(int i = 0; i < 32; i++) {
        sock->conn_queue[i] = NULL;
    }
    sock->conn_queue_front = 0;
    sock->conn_queue_back = 0;
    sock->conn_queue_size = 0;
    
    printf("Socket %d listening with backlog=%d\n", sockfd, backlog);
    return 0;
}

// Accept a connection on socket
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(sock->type != SOCKET_TCP) {
        return -1; // Only TCP sockets can accept
    }
    
    if(!sock->listening) {
        return -1; // Must be listening
    }
    
    // Check if there are pending connections
    if(sock->conn_queue_size == 0) {
        if(sock->options.nonblock) {
            return -1; // Would block
        }
        
        // In a real implementation, this would block until a connection is available
        // For now, return error
        return -1;
    }
    
    // Get the first pending connection
    socket_t* client_sock = sock->conn_queue[sock->conn_queue_front];
    sock->conn_queue[sock->conn_queue_front] = NULL;
    sock->conn_queue_front = (sock->conn_queue_front + 1) % 32;
    sock->conn_queue_size--;
    
    // Fill in client address if requested
    if(addr && addrlen) {
        struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
        addr_in->sin_family = AF_INET;
        addr_in->sin_port = client_sock->remote_port;
        addr_in->sin_addr.s_addr = client_sock->remote_ip;
        
        if(*addrlen > sizeof(struct sockaddr_in)) {
            *addrlen = sizeof(struct sockaddr_in);
        }
    }
    
    printf("Connection accepted on socket %d, client socket %d\n", sockfd, client_sock->fd);
    return client_sock->fd;
}

// Send data on socket
int send(int sockfd, const void* buf, size_t len, int flags) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(!sock->connected) {
        return -1; // Not connected
    }
    
    if(!buf || len == 0) {
        return 0;
    }
    
    if(sock->type == SOCKET_TCP) {
        // For TCP, try to send data
        if(sock->send_buffer_used + len > sock->send_buffer_size) {
            if(sock->options.nonblock) {
                return -1; // Would block
            }
            // In a real implementation, this would wait for buffer space
            // For now, return error
            return -1;
        }
        
        // Copy data to send buffer
        size_t pos = (sock->send_buffer_pos + sock->send_buffer_used) % sock->send_buffer_size;
        if(pos + len <= sock->send_buffer_size) {
            // Data fits in one piece
            memcpy(sock->send_buffer + pos, buf, len);
        } else {
            // Data wraps around
            size_t first_part = sock->send_buffer_size - pos;
            memcpy(sock->send_buffer + pos, buf, first_part);
            memcpy(sock->send_buffer, (char*)buf + first_part, len - first_part);
        }
        
        sock->send_buffer_used += len;
        
        // Actually send the data over network (simulate)
        // In real implementation, this would call TCP layer
        printf("TCP send: %d bytes to %x:%d\n", len, sock->remote_ip, sock->remote_port);
        
        return len;
    } else if(sock->type == SOCKET_UDP) {
        // For UDP, send directly
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = sock->remote_port;
        dest_addr.sin_addr.s_addr = sock->remote_ip;
        
        return sendto(sockfd, buf, len, flags, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    }
    
    return -1; // Unknown socket type
}

// Receive data from socket
int recv(int sockfd, void* buf, size_t len, int flags) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(!sock->connected) {
        return -1; // Not connected
    }
    
    if(!buf || len == 0) {
        return 0;
    }
    
    if(sock->type == SOCKET_TCP) {
        // Check if there's data to receive
        if(sock->recv_buffer_used == 0) {
            if(sock->options.nonblock) {
                return 0; // No data available
            }
            // In a real implementation, this would block until data arrives
            // For now, return 0
            return 0;
        }
        
        // Copy data from receive buffer
        size_t bytes_to_copy = (len < sock->recv_buffer_used) ? len : sock->recv_buffer_used;
        size_t pos = sock->recv_buffer_pos;
        
        if(pos + bytes_to_copy <= sock->recv_buffer_size) {
            // Data fits in one piece
            memcpy(buf, sock->recv_buffer + pos, bytes_to_copy);
        } else {
            // Data wraps around
            size_t first_part = sock->recv_buffer_size - pos;
            memcpy(buf, sock->recv_buffer + pos, first_part);
            memcpy((char*)buf + first_part, sock->recv_buffer, bytes_to_copy - first_part);
        }
        
        // Update buffer position and usage
        sock->recv_buffer_pos = (sock->recv_buffer_pos + bytes_to_copy) % sock->recv_buffer_size;
        sock->recv_buffer_used -= bytes_to_copy;
        
        printf("TCP recv: %d bytes\n", bytes_to_copy);
        return bytes_to_copy;
    } else if(sock->type == SOCKET_UDP) {
        // For UDP, use recvfrom
        struct sockaddr_in src_addr;
        socklen_t addrlen = sizeof(src_addr);
        return recvfrom(sockfd, buf, len, flags, (struct sockaddr*)&src_addr, &addrlen);
    }
    
    return -1; // Unknown socket type
}

// Send data to specific address (UDP)
int sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(!buf || len == 0) {
        return 0;
    }
    
    if(sock->type == SOCKET_UDP) {
        if(!dest_addr || addrlen < sizeof(struct sockaddr_in)) {
            return -1;
        }
        
        struct sockaddr_in* addr_in = (struct sockaddr_in*)dest_addr;
        
        // For UDP, send directly via UDP layer
        // This is a simplified implementation - in reality, this would call the UDP module
        printf("UDP sendto: %d bytes to %x:%d\n", len, addr_in->sin_addr.s_addr, addr_in->sin_port);
        
        // Simulate sending via IP layer
        // In real implementation: return udp_sendto(sock, buf, len, addr_in);
        return len;
    } else if(sock->type == SOCKET_TCP) {
        // If TCP socket and remote address is specified, this is an error
        if(dest_addr) {
            return -1; // TCP sockets don't use sendto with address
        }
        return send(sockfd, buf, len, flags);
    }
    
    return -1;
}

// Receive data from specific address (UDP)
int recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(!buf || len == 0) {
        return 0;
    }
    
    if(sock->type == SOCKET_UDP) {
        // Receive data (simplified)
        // In real implementation, this would pull from UDP packet queue
        printf("UDP recvfrom called, but packet queueing not implemented\n");
        
        // Fill in source address if requested
        if(src_addr && addrlen) {
            struct sockaddr_in* addr_in = (struct sockaddr_in*)src_addr;
            addr_in->sin_family = AF_INET;
            addr_in->sin_port = 0; // Unknown
            addr_in->sin_addr.s_addr = 0; // Unknown
            
            if(*addrlen > sizeof(struct sockaddr_in)) {
                *addrlen = sizeof(struct sockaddr_in);
            }
        }
        
        return 0; // No data available
    } else if(sock->type == SOCKET_TCP) {
        // For TCP, this behaves like recv
        return recv(sockfd, buf, len, flags);
    }
    
    return -1;
}

// Shutdown socket operations
int shutdown(int sockfd, int how) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    // Update socket state based on how parameter
    switch(how) {
        case SHUT_RD:
            // Stop receiving data
            break;
        case SHUT_WR:
            // Stop sending data
            break;
        case SHUT_RDWR:
            // Stop both sending and receiving
            sock->closed = 1;
            sock->state = SS_DISCONNECTING;
            break;
        default:
            return -1;
    }
    
    printf("Socket %d shut down (%d)\n", sockfd, how);
    return 0;
}

// Close socket
int closesocket(int sockfd) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    // Remove from socket list
    if(socket_list == sock) {
        socket_list = sock->next;
    } else {
        socket_t* prev = socket_list;
        while(prev && prev->next != sock) {
            prev = prev->next;
        }
        if(prev) {
            prev->next = sock->next;
        }
    }
    
    // Free buffers
    if(sock->send_buffer) {
        kfree(sock->send_buffer);
    }
    if(sock->recv_buffer) {
        kfree(sock->recv_buffer);
    }
    
    // Free socket structure
    kfree(sock);
    
    printf("Socket %d closed\n", sockfd);
    return 0;
}

// Get socket option
int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(!optval || !optlen) {
        return -1;
    }
    
    switch(level) {
        case SOL_SOCKET:
            switch(optname) {
                case SO_REUSEADDR:
                    if(*optlen >= sizeof(int)) {
                        *(int*)optval = sock->options.reuseaddr;
                        *optlen = sizeof(int);
                        return 0;
                    }
                    break;
                case SO_BROADCAST:
                    if(*optlen >= sizeof(int)) {
                        *(int*)optval = sock->options.broadcast;
                        *optlen = sizeof(int);
                        return 0;
                    }
                    break;
                case SO_KEEPALIVE:
                    if(*optlen >= sizeof(int)) {
                        *(int*)optval = sock->options.keepalive;
                        *optlen = sizeof(int);
                        return 0;
                    }
                    break;
                case SO_SNDBUF:
                    if(*optlen >= sizeof(int)) {
                        *(int*)optval = sock->options.sndbuf;
                        *optlen = sizeof(int);
                        return 0;
                    }
                    break;
                case SO_RCVBUF:
                    if(*optlen >= sizeof(int)) {
                        *(int*)optval = sock->options.rcvbuf;
                        *optlen = sizeof(int);
                        return 0;
                    }
                    break;
                case SO_LINGER:
                    if(*optlen >= sizeof(struct linger)) {
                        *(struct linger*)optval = sock->options.linger_opt;
                        *optlen = sizeof(struct linger);
                        return 0;
                    }
                    break;
            }
            break;
    }
    
    return -1;
}

// Set socket option
int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(!optval) {
        return -1;
    }
    
    switch(level) {
        case SOL_SOCKET:
            switch(optname) {
                case SO_REUSEADDR:
                    if(optlen >= sizeof(int)) {
                        sock->options.reuseaddr = *(const int*)optval;
                        return 0;
                    }
                    break;
                case SO_BROADCAST:
                    if(optlen >= sizeof(int)) {
                        sock->options.broadcast = *(const int*)optval;
                        return 0;
                    }
                    break;
                case SO_KEEPALIVE:
                    if(optlen >= sizeof(int)) {
                        sock->options.keepalive = *(const int*)optval;
                        return 0;
                    }
                    break;
                case SO_SNDBUF:
                    if(optlen >= sizeof(int)) {
                        sock->options.sndbuf = *(const int*)optval;
                        if(sock->send_buffer) {
                            // Reallocate buffer if already allocated
                            kfree(sock->send_buffer);
                            sock->send_buffer_size = sock->options.sndbuf;
                            sock->send_buffer = (uint8_t*)kmalloc(sock->send_buffer_size);
                            if(!sock->send_buffer) {
                                sock->send_buffer_size = 0;
                                return -1;
                            }
                        }
                        return 0;
                    }
                    break;
                case SO_RCVBUF:
                    if(optlen >= sizeof(int)) {
                        sock->options.rcvbuf = *(const int*)optval;
                        if(sock->recv_buffer) {
                            // Reallocate buffer if already allocated
                            kfree(sock->recv_buffer);
                            sock->recv_buffer_size = sock->options.rcvbuf;
                            sock->recv_buffer = (uint8_t*)kmalloc(sock->recv_buffer_size);
                            if(!sock->recv_buffer) {
                                sock->recv_buffer_size = 0;
                                return -1;
                            }
                        }
                        return 0;
                    }
                    break;
                case SO_LINGER:
                    if(optlen >= sizeof(struct linger)) {
                        sock->options.linger_opt = *(const struct linger*)optval;
                        return 0;
                    }
                    break;
            }
            break;
    }
    
    return -1;
}

// Select function (simplified)
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout) {
    int ready_fds = 0;
    
    // This is a simplified implementation
    // In a real implementation, this would check for ready sockets
    // and potentially block until timeout
    
    if(readfds) {
        for(int i = 0; i < nfds; i++) {
            if(FD_ISSET(i, readfds)) {
                socket_t* sock = socket_find_by_fd(i);
                if(sock && sock->type == SOCKET_TCP && sock->recv_buffer_used > 0) {
                    // Mark as ready for reading
                    ready_fds++;
                } else {
                    FD_CLR(i, readfds); // Clear if not ready
                }
            }
        }
    }
    
    if(writefds) {
        for(int i = 0; i < nfds; i++) {
            if(FD_ISSET(i, writefds)) {
                socket_t* sock = socket_find_by_fd(i);
                if(sock && sock->type == SOCKET_TCP && 
                   sock->send_buffer_used < sock->send_buffer_size) {
                    // Mark as ready for writing
                    ready_fds++;
                } else {
                    FD_CLR(i, writefds); // Clear if not ready
                }
            }
        }
 // exceptfds typically not used in simple implementations
    if(exceptfds) {
        FD_ZERO(exceptfds); // Clear exception set
    }
    
    return ready_fds;
}

// ioctl for socket (simplified)
int ioctlsocket(int sockfd, long cmd, unsigned long* argp) {
    socket_t* sock = socket_find_by_fd(sockfd);
    if(!sock) {
        return -1;
    }
    
    if(!argp) {
        return -1;
    }
    
    switch(cmd) {
        case 0x8004667F: // FIONBIO - set non-blocking
            sock->options.nonblock = (*argp != 0);
            return 0;
        default:
            return -1;
    }
}

