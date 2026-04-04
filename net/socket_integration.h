#ifndef SOCKET_INTEGRATION_H
#define SOCKET_INTEGRATION_H

#include "socket.h"
#include "tcp.h"
#include "udp.h"

// Integration functions that connect socket layer to TCP/UDP layers

// Handle incoming TCP packet
void socket_tcp_handle_packet(struct ip_header* iph, struct tcp_header* tcph, size_t tcp_len);

// Handle incoming UDP packet
void socket_udp_handle_packet(struct ip_header* iph, struct udp_header* udph, size_t udp_len);

// Helper functions for TCP connection management
void socket_tcp_data_received(socket_t* sock, const void* data, size_t len);
void socket_tcp_connection_established(socket_t* sock);
void socket_tcp_connection_closed(socket_t* sock);

#endif

