#include "tcp.h"
#include "socket.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "mm.h"

// Global variables
struct tcp_socket* tcp_sockets = NULL;
static uint16_t next_local_port = 1024;

// Calculate TCP checksum
uint16_t tcp_checksum(struct tcp_header* tcph, size_t tcp_len, uint32_t src_ip, uint32_t dst_ip) {
    struct tcp_pseudo_header pseudo_hdr;
    pseudo_hdr.src_ip = src_ip;
    pseudo_hdr.dst_ip = dst_ip;
    pseudo_hdr.reserved = 0;
    pseudo_hdr.protocol = 6; // IP_PROTO_TCP
    pseudo_hdr.tcp_length = tcp_len;
    
    size_t total_len = sizeof(struct tcp_pseudo_header) + tcp_len;
    uint8_t* checksum_data = kmalloc(total_len);
    
    if(!checksum_data) return 0;
    
    memcpy(checksum_data, &pseudo_hdr, sizeof(struct tcp_pseudo_header));
    memcpy(checksum_data + sizeof(struct tcp_pseudo_header), tcph, tcp_len);
    
    uint16_t result = calculate_checksum(checksum_data, total_len);
    
    kfree(checksum_data);
    return result;
}

// Initialize TCP
void tcp_init(void) {
    tcp_sockets = NULL;
    printf("TCP stack initialized\n");
}

// Create TCP buffer
struct tcp_buffer* tcp_create_buffer(size_t size) {
    struct tcp_buffer* buf = (struct tcp_buffer*)kmalloc(sizeof(struct tcp_buffer));
    if(!buf) return NULL;
    
    buf->data = (uint8_t*)kmalloc(size);
    if(!buf->data) {
        kfree(buf);
        return NULL;
    }
    
    buf->size = size;
    buf->head = 0;
    buf->tail = 0;
    buf->used = 0;
    
    return buf;
}

// Enqueue data to buffer
int tcp_buffer_enqueue(struct tcp_buffer* buf, const void* data, size_t len) {
    if(buf->size - buf->used < len) {
        return -1; // Not enough space
    }
    
    size_t space_to_end = buf->size - buf->tail;
    if(space_to_end >= len) {
        memcpy(buf->data + buf->tail, data, len);
    } else {
        memcpy(buf->data + buf->tail, data, space_to_end);
        memcpy(buf->data, (char*)data + space_to_end, len - space_to_end);
    }
    
    buf->tail = (buf->tail + len) % buf->size;
    buf->used += len;
    
    return 0;
}

// Dequeue data from buffer
size_t tcp_buffer_dequeue(struct tcp_buffer* buf, void* data, size_t len) {
    if(buf->used == 0) {
        return 0;
    }
    
    size_t bytes_to_copy = (len < buf->used) ? len : buf->used;
    size_t space_to_end = buf->size - buf->head;
    
    if(space_to_end >= bytes_to_copy) {
        memcpy(data, buf->data + buf->head, bytes_to_copy);
    } else {
        memcpy(data, buf->data + buf->head, space_to_end);
        memcpy((char*)data + space_to_end, buf->data, bytes_to_copy - space_to_end);
    }
    
    buf->head = (buf->head + bytes_to_copy) % buf->size;
    buf->used -= bytes_to_copy;
    
    return bytes_to_copy;
}

// Create TCP socket
int tcp_socket(int domain, int type, int protocol) {
    if(domain != AF_INET || type != SOCK_STREAM) {
        return -1;
    }
    
    struct tcp_socket* sock = (struct tcp_socket*)kmalloc(sizeof(struct tcp_socket));
    if(!sock) return -1;
    
    memset(sock, 0, sizeof(struct tcp_socket));
    
    // Initialize default parameters
    sock->state = TCP_CLOSED;
    sock->type = TCP_SOCKET_ACTIVE;
    sock->local_port = next_local_port++;
    sock->our_window = 4096; // Our window size
    sock->advertised_window = 4096;
    sock->mss = 1460; // MSS for Ethernet
    sock->rtt = 1000; // 1 second default
    sock->rto = 3000; // 3 seconds retransmission timeout
    
    // Create buffers
    sock->send_buf = tcp_create_buffer(8192);
    sock->recv_buf = tcp_create_buffer(8192);
    
    if(!sock->send_buf || !sock->recv_buf) {
        tcp_cleanup_socket(sock);
        return -1;
    }
    
    // Add to global list
    sock->next = tcp_sockets;
    tcp_sockets = sock;
    
    return (int)sock;
}

// Bind socket to port
int tcp_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    struct tcp_socket* sock = (struct tcp_socket*)sockfd;
    if(!sock) return -1;
    
    struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
    sock->local_port = addr_in->sin_port;
    sock->local_ip = addr_in->sin_addr.s_addr;
    
    return 0;
}

// Listen on socket
int tcp_listen(int sockfd, int backlog) {
    struct tcp_socket* sock = (struct tcp_socket*)sockfd;
    if(!sock) return -1;
    
    sock->type = TCP_SOCKET_PASSIVE;
    sock->state = TCP_LISTEN;
    
    return 0;
}

// Accept connection
int tcp_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    struct tcp_socket* listen_sock = (struct tcp_socket*)sockfd;
    if(!listen_sock || listen_sock->type != TCP_SOCKET_PASSIVE) return -1;
    
    // Wait for new connection (simplified)
    if(listen_sock->accept_queue_count == 0) {
        return -1; // No connections available
    }
    
    struct tcp_socket* accepted_sock = listen_sock->accept_queue[listen_sock->accept_queue_head];
    listen_sock->accept_queue_head = (listen_sock->accept_queue_head + 1) % 10;
    listen_sock->accept_queue_count--;
    
    if(addr && addrlen) {
        struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
        addr_in->sin_family = AF_INET;
        addr_in->sin_port = accepted_sock->remote_port;
        addr_in->sin_addr.s_addr = accepted_sock->remote_ip;
        *addrlen = sizeof(struct sockaddr_in);
    }
    
    return (int)accepted_sock;
}

// Connect to remote address
int tcp_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    struct tcp_socket* sock = (struct tcp_socket*)sockfd;
    if(!sock || sock->state != TCP_CLOSED) return -1;
    
    struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
    sock->remote_ip = addr_in->sin_addr.s_addr;
    sock->remote_port = addr_in->sin_port;
    sock->type = TCP_SOCKET_ACTIVE;
    
    // Generate random initial sequence number
    sock->send_seq = (uint32_t)((uint64_t)sockfd * 0x12345678);
    sock->recv_seq = 0;
    
    // Send SYN
    int result = tcp_send_segment(sock, TCP_FLAG_SYN, NULL, 0);
    if(result >= 0) {
        sock->state = TCP_SYN_SENT;
    }
    
    return result;
}

// Send data
int tcp_send(int sockfd, const void* buf, size_t len, int flags) {
    struct tcp_socket* sock = (struct tcp_socket*)sockfd;
    if(!sock || sock->state != TCP_ESTABLISHED) return -1;
    
    // Add data to send buffer
    if(tcp_buffer_enqueue(sock->send_buf, buf, len) < 0) {
        return -1;
    }
    
    // Try to send data immediately
    size_t bytes_sent = 0;
    while(bytes_sent < len) {
        size_t chunk_size = (len - bytes_sent < sock->mss) ? (len - bytes_sent) : sock->mss;
        
        // Check window size
        if(sock->send_seq - sock->send_ack >= sock->advertised_window) {
            // Window full, wait for ACK
            break;
        }
        
        int result = tcp_send_segment(sock, TCP_FLAG_PSH | TCP_FLAG_ACK, 
                                     (char*)buf + bytes_sent, chunk_size);
        if(result < 0) break;
        
        bytes_sent += chunk_size;
        sock->send_seq += chunk_size;
    }
    
    return bytes_sent;
}

// Receive data
int tcp_recv(int sockfd, void* buf, size_t len, int flags) {
    struct tcp_socket* sock = (struct tcp_socket*)sockfd;
    if(!sock) return -1;
    
    // Wait for data in buffer (simplified)
    if(sock->recv_buf->used == 0) {
        return 0; // No data available
    }
    
    return tcp_buffer_dequeue(sock->recv_buf, buf, len);
}

// Close socket
int tcp_close(int sockfd) {
    struct tcp_socket* sock = (struct tcp_socket*)sockfd;
    if(!sock) return -1;
    
    if(sock->state == TCP_ESTABLISHED) {
        // Send FIN
        tcp_send_segment(sock, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
        sock->state = TCP_FIN_WAIT_1;
    } else if(sock->state == TCP_LISTEN) {
        // Close all pending connections
        for(int i = 0; i < 10; i++) {
            if(sock->accept_queue[i]) {
                tcp_close((int)sock->accept_queue[i]);
            }
        }
    }
    
    // Remove from global list
    if(tcp_sockets == sock) {
        tcp_sockets = sock->next;
    } else {
        struct tcp_socket* current = tcp_sockets;
        while(current && current->next != sock) {
            current = current->next;
        }
        if(current) {
            current->next = sock->next;
        }
    }
    
    tcp_cleanup_socket(sock);
    return 0;
}

// Clean up socket resources
void tcp_cleanup_socket(struct tcp_socket* sock) {
    if(!sock) return;
    
    if(sock->send_buf) {
        if(sock->send_buf->data) kfree(sock->send_buf->data);
        kfree(sock->send_buf);
    }
    
    if(sock->recv_buf) {
        if(sock->recv_buf->data) kfree(sock->recv_buf->data);
        kfree(sock->recv_buf);
    }
    
    // Clean up outstanding segments
    while(sock->outstanding_segments) {
        struct tcp_outstanding_segment* seg = sock->outstanding_segments;
        sock->outstanding_segments = seg->next;
        if(seg->data) kfree(seg->data);
        kfree(seg);
    }
    
    kfree(sock);
}

// Send TCP segment
int tcp_send_segment(struct tcp_socket* sock, uint8_t flags, const void* data, size_t len) {
    size_t packet_size = TCP_HEADER_MIN_LEN + len;
    struct tcp_header* tcph = (struct tcp_header*)kmalloc(packet_size);
    
    if(!tcph) return -1;
    
    memset(tcph, 0, TCP_HEADER_MIN_LEN);
    tcph->src_port = sock->local_port;
    tcph->dst_port = sock->remote_port;
    tcph->seq_num = sock->send_seq;
    tcph->ack_num = sock->recv_seq;
    tcph->data_offset_reserved = (TCP_HEADER_MIN_LEN / 4) << 4;
    tcph->flags = flags;
    tcph->window_size = sock->our_window;
    tcph->urgent_ptr = 0;
    
    if(data && len > 0) {
        memcpy((char*)tcph + TCP_HEADER_MIN_LEN, data, len);
    }

    tcph->checksum = 0;
    tcph->checksum = tcp_checksum(tcph, packet_size, sock->local_ip, sock->remote_ip);
    
    // Send via IP layer (placeholder - implement actual IP sending)
    printf("TCP: Sending %d bytes to %x:%d (flags: %d)\n", len, sock->remote_ip, sock->remote_port, flags);
    
    // If this is data segment, add to outstanding table
    if((flags & (TCP_FLAG_PSH | TCP_FLAG_SYN | TCP_FLAG_FIN)) && packet_size > 0) {
        tcp_enqueue_segment(sock, sock->send_seq, data, len);
    }
    
    kfree(tcph);
    return 0;
}

// Add segment to outstanding table
int tcp_enqueue_segment(struct tcp_socket* sock, uint32_t seq_num, const void* data, size_t len) {
    struct tcp_outstanding_segment* seg = (struct tcp_outstanding_segment*)kmalloc(sizeof(struct tcp_outstanding_segment));
    if(!seg) return -1;
    
    seg->seq_num = seq_num;
    seg->len = len;
    seg->sent_time = 0; // Use actual time
    seg->retransmitted = 0;
    
    if(data && len > 0) {
        seg->data = kmalloc(len);
        if(!seg->data) {
            kfree(seg);
            return -1;
        }
        memcpy(seg->data, data, len);
    } else {
        seg->data = NULL;
    }
    
    // Add to beginning of list
    seg->next = sock->outstanding_segments;
    sock->outstanding    
    return 0;
}

// Remove acknowledged segment from table
int tcp_dequeue_segment(struct tcp_socket* sock, uint32_t ack_num) {
    if(!sock->outstanding_segments) return -1;
    
    struct tcp_outstanding_segment** prev_ptr = &sock->outstanding_segments;
    struct tcp_outstanding_segment* current = sock->outstanding_segments;
    
    while(current) {
        if(current->seq_num + current->len <= ack_num) {
            // This segment is acknowledged, remove it
            *prev_ptr = current->next;
            
            if(current->data) kfree(current->data);
            kfree(current);
            
            current = *prev_ptr;
        } else {
            prev_ptr = &current->next;
            current = current->next;
        }
    }
    
    return 0;
}

// Handle incoming TCP packet
void tcp_handle_packet(void* ip_payload, size_t payload_len, uint32_t src_ip, uint32_t dst_ip) {
    struct tcp_header* tcph = (struct tcp_header*)ip_payload;
    
    uint16_t tcp_len = payload_len;
    uint16_t data_offset = (tcph->data_offset_reserved >> 4) * 4;
    size_t data_len = tcp_len - data_offset;
    
    uint16_t src_port = tcph->src_port;
    uint16_t dst_port = tcph->dst_port;
    uint32_t seq_num = tcph->seq_num;
    uint32_t ack_num = tcph->ack_num;
    uint8_t flags = tcph->flags;
    
    // Check checksum
    uint16_t received_checksum = tcph->checksum;
    tcph->checksum = 0;
    uint16_t calculated_checksum = tcp_checksum(tcph, tcp_len, src_ip, dst_ip);
    
    if(calculated_checksum != 0) {
        printf("TCP checksum error\n");
        return;
    }
    
    // Find matching socket
    struct tcp_socket* sock = NULL;
    
    // First search by local port
    struct tcp_socket* current = tcp_sockets;
    while(current) {
        if(current->local_port == dst_port) {
            if(current->type == TCP_SOCKET_PASSIVE) {
                // For listening socket, check if there's already a connection
                // or this is a new connection request
                sock = current;
                break;
            } else if(current->remote_port == src_port && 
                     current->remote_ip == src_ip) {
                sock = current;
                break;
            }
        }
        current = current->next;
    }
    
    if(!sock) {
        // No matching socket, send RST
        printf("TCP: No matching socket for %x:%d -> %x:%d\n", src_ip, src_port, dst_ip, dst_port);
        return;
    }
    
    // Update remote info
    if(sock->remote_ip == 0) {
        sock->remote_ip = src_ip;
        sock->remote_port = src_port;
    }
    
    // Handle based on state and flags
    switch(sock->state) {
        case TCP_LISTEN:
            if(flags & TCP_FLAG_SYN) {
                // New connection
                sock->recv_seq = seq_num + 1;
                sock->send_seq = (uint32_t)0x12345678; // Random ISN
                
                // Send SYN-ACK
                tcp_send_segment(sock, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
                sock->send_seq++; // Increment after sending SYN
                
                // Create new socket for this connection
                struct tcp_socket* new_sock = (struct tcp_socket*)kmalloc(sizeof(struct tcp_socket));
                if(new_sock) {
                    memcpy(new_sock, sock, sizeof(struct tcp_socket));
                    new_sock->state = TCP_SYN_RECEIVED;
                    new_sock->remote_ip = src_ip;
                    new_sock->remote_port = src_port;
                    new_sock->recv_seq = seq_num + 1;
                    new_sock->send_seq = sock->send_seq;
                    
                    // Add to accept queue
                    if(sock->accept_queue_count < 10) {
                        int idx = (sock->accept_queue_head + sock->accept_queue_count) % 10;
                        sock->accept_queue[idx] = new_sock;
                        sock->accept_queue_count++;
                    }
                }
            }
            break;
            
        case TCP_SYN_SENT:
            if(flags & TCP_FLAG_SYN) {
                if(flags & TCP_FLAG_ACK) {
                    // SYN-ACK received
                    sock->recv_seq = seq_num + 1;
                    sock->send_seq = ack_num;
                    
                    // Send ACK
                    tcp_send_segment(sock, TCP_FLAG_ACK, NULL, 0);
                    sock->state = TCP_ESTABLISHED;
                } else {
                    // Only SYN, send SYN-ACK and transition to SYN_RECEIVED
                    sock->recv_seq = seq_num + 1;
                    tcp_send_segment(sock, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
                    sock->send_seq++; // Increment after sending SYN
                    sock->state = TCP_SYN_RECEIVED;
                }
            }
            break;
            
        case TCP_SYN_RECEIVED:
            if(flags & TCP_FLAG_ACK) {
                sock->state = TCP_ESTABLISHED;
            }
            break;
            
        case TCP_ESTABLISHED:
            if(flags & TCP_FLAG_FIN) {
                // Remote side is closing connection
                sock->recv_seq = seq_num + 1;
                
                // Send ACK
                tcp_send_segment(sock, TCP_FLAG_ACK, NULL, 0);
                
                sock->state = TCP_CLOSE_WAIT;
                
                // Respond with FIN
                tcp_send_segment(sock, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
                sock->state = TCP_LAST_ACK;
            } else if(flags & TCP_FLAG_RST) {
                // Connection reset
                sock->state = TCP_CLOSED;
            } else if(data_len > 0 && seq_num == sock->recv_seq) {
                // Received data
                char* data = (char*)tcph + data_offset;
                
                // Add data to receive buffer
                if(sock->recv_buf->size - sock->recv_buf->used >= data_len) {
                    tcp_buffer_enqueue(sock->recv_buf, data, data_len);
                    sock->recv_seq += data_len;
                    
                    // Send ACK
                    tcp_send_segment(sock, TCP_FLAG_ACK, NULL, 0);
                }
            } else if(flags & TCP_FLAG_ACK) {
                // ACK received
                tcp_dequeue_segment(sock, ack_num);
                sock->send_ack = ack_num;
            }
            break;
            
        case TCP_FIN_WAIT_1:
            if(flags & TCP_FLAG_ACK) {
                sock->state = TCP_FIN_WAIT_2;
            } else if(flags & TCP_FLAG_FIN) {
                sock->recv_seq = seq_num + 1;
                tcp_send_segment(sock, TCP_FLAG_ACK, NULL, 0);
                sock->state = TCP_CLOSING;
            }
            break;
            
        case TCP_FIN_WAIT_2:
            if(flags & TCP_FLAG_FIN) {
                sock->recv_seq = seq_num + 1;
                tcp_send_segment(sock, TCP_FLAG_ACK, NULL, 0);
                sock->state = TCP_TIME_WAIT;
            }
            break;
            
        case TCP_CLOSING:
            if(flags & TCP_FLAG_ACK) {
                sock->state = TCP_TIME_WAIT;
            }
            break;
            
        case TCP_TIME_WAIT:
            // Wait for timeout or handle duplicate FIN
            break;
            
        case TCP_CLOSE_WAIT:
            if(flags & TCP_FLAG_ACK) {
                sock->state = TCP_CLOSED;
            }
            break;
            
        case TCP_LAST_ACK:
            if(flags & TCP_FLAG_ACK) {
                sock->state = TCP_CLOSED;
            }
            break;
    }
    
    // Update advertised window
    sock->advertised_window = tcph->window_size;
}

