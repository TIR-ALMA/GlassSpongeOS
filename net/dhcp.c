#include "dhcp.h"
#include "udp.h"
#include "arp.h"
#include "lib/printf.h"
#include "mm.h"

static struct socket* dhcp_sock = NULL;

int dhcp_request(void) {
    if(!dhcp_sock) {
        dhcp_sock = (struct socket*)udp_socket(AF_INET, SOCK_DGRAM, 0);
        if(!dhcp_sock) return -1;
        dhcp_sock->local_port = htons(68);
    }

    struct {
        uint8_t op;
        uint8_t htype;
        uint8_t hlen;
        uint8_t hops;
        uint32_t xid;
        uint16_t secs;
        uint16_t flags;
        uint32_t ciaddr;
        uint32_t yiaddr;
        uint32_t siaddr;
        uint32_t giaddr;
        uint8_t chaddr[16];
        uint8 sname[64];
        uint8_t file[128];
        uint32_t magic;
        uint8_t options[312];
    } __attribute__((packed)) pkt;

    memset(&pkt, 0, sizeof(pkt));
    pkt.op = 1; // BOOTREQUEST
    pkt.htype = 1; // Ethernet
    pkt.hlen = 6;
    pkt.xid = htonl(0x12345678);
    pkt.flags = htons(0x8000); // broadcast
    memcpy(pkt.chaddr, net_local_mac, 6);
    pkt.magic = htonl(0x63825363); // DHCP uint8_t* opt = pkt.options;
    *opt++ = 53; *opt++ = 1; *opt++ = 1; // DHCP Discover
    *opt++ = 55; *opt++ = 3; *opt++ = 1; *opt++ = 3; *opt++ = 6; // req opts
    *opt++ = 255; // end

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(67    server.sin_addr.s_addr = htonl(0xFFFFFFFF); // broadcast

    udp_sendto(dhcp_sock->fd, &pkt, sizeof(pkt) - sizeof(pkt.options) + (opt - pkt.options),
               0, (struct sockaddr*)&server, sizeof(server));
    return 0;
}
