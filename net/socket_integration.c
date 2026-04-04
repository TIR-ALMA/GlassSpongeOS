#include "socket.h"
#include "tcp.h"
#include "udp.h"

// Integration functions that connect socket layer to TCP/UDP layers

void socket_tcp_handle_packet(struct ip_header* iph, struct tcp_header* tcph, size_t tcp_len) {
    uint16_t src_port = ntohs(tcph->src_port);
    uint16_t dst_port = ntohs(tcph->dst_port);
    uint32_t src_ip = ntohl(iph->src_ip);
    uint32_t dst_ip = ntohl(iph->dst_ip);
    
    // Find matching socket
    socket_t* sock = socket_list;
    while(sock) {
        if(sock->type == SOCKET_TCP && 
           sock->local_port == dst_port && 
           (sock->remote_port == 0 || sock->remote_port == src_port) &&
           (sock->remote_ip == 0 || sock->remote_ip == src_ip)) {
            break;
        }
        sock = sock->next;
    }
    
    if(!sock) {
        // No matching socket - could be a listening socket
        if(tcph->flags & TCP_FLAG_SYN) {
            // Find listening socket
            socket_t* listener = socket_list;
            while(listener) {
                if(listener->type == SOCKET_TCP && 
                   listener->listening && 
                   listener->local_port == dst_port) {
                    // Create new socket for this connection
                    socket_t* new_sock = (socket_t*)kmalloc(sizeof(socket_t));
                    if(new_sock) {
                        memcpy(new_sock, listener, sizeof(socket_t));
                        new_sock->fd = socket_allocate_fd();
                        new_sock->local_port = dst_port;
                        new_sock->remote_port = src_port;
                        new_sock->remote_ip = src_ip;
                        new_sock->connected = 1;
                        new_sock->listening = 0;
                        
                        // Add to connection queue
                        if(listener->conn_queue_size < 32) {
                            listener->conn_queue[listener->conn_queue_back] = new_sock;
                            listener->conn_queue_back = (listener->conn_queue_back + 1) % 32;
                            listener->conn_queue_size++;
                            
                            // Add to global socket list
                            new_sock->next = socket_list;
                            socket_list = new_sock;
                        } else {
                            k                        }
                    }
                    break;
                }
                listener = listener->next;
            }
        }
        return;
    }
    
    // Process TCP flags
    if(tcph->flags & TCP_FLAG_RST) {
        socket_tcp_connection_closed(sock);
        return;
    }
    
    if(tcph->flags & TCP_FLAG_FIN) {
        // Handle FIN - connection closing
        sock->connected = 0;
        sock->state = SS_DISCONNECTING;
        return;
    }
    
    if(tcph->flags & TCP_FLAG_ACK) {
        // Update sequence number
        sock ntohl(tcph->ack_num);
    }
    
    // Process data if present
    size_t data_offset = (tcph->data_offset_reserved >> 4) * 4;
    size_t data_len = tcp_len - data_offset;
    
    if(data_len > 0 && sock->connected) {
        char* data = (char*)tcph + data_offset;
        socket_tcp_data_received(sock, data, data_len);
    }
}

void socket_udp_handle_packet(struct ip_header* iph* udph, size_t udp_len) {
    uint16_t src_port = ntohs(udph->src_port);
    uint16_t dst_port = ntohs(udph->dst_port);
    uint32_t src_ip = ntohl(iph->src_ip);
    uint32_t dst_ip = ntohl(iph->dst_ip);
    
    // Find matching socket
    socket_t* sock = socket_list;
    while(sock) {
        if(sock->type == SOCKET_UDP && 
           sock->local_port == dst_port) {
            break;
        }
        sock = sock->next;
    }
    
    if(!sock) {
        return; // No matching socket
    }
    
    // Process UDP data
    size_t data_len = ntohs(udph->length) - sizeof(struct udp_header);
    char* data = (char*)udph + sizeof(struct udp_header);
    
    // For UDP, we'd normally queue the packet with source info
    // In this simplified implementation, we'll just print it
    printf("UDP packet received on socket %d: %d bytes from %x:%d\n", 
           sock->fd, data_len, src_ip, src_port);
}
