// net/icmp.c
#include "icmp.h"
#include "ip.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "mm.h"

void icmp_init(void) {
    printf("[ICMP] Initializing ICMP protocol stack\n");
}

uint16_t icmp_calculate_checksum(struct icmp_header* icmph, size_t total_len) {
    uint16_t* ptr = (uint16_t*)icmph;
    uint32_t sum = 0;
    size_t i;

    for (i = 0; i < total_len / 2; i++) {
        sum += ntohs(ptr[i]);
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }

    if (total_len & 1) {
        sum += ((uint8_t*)icmph)[total_len - 1] << 8;
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }

    return ~sum;
}

int icmp_send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t seq, void* data, size_t data_len) {
    size_t packet_size = sizeof(struct icmp_header) + data_len;
    struct icmp_header* icmph = (struct icmp_header*)kmalloc(packet_size);

    if (!icmph) {
        printf("[ICMP] Failed to allocate memory for echo request\n");
        return -1;
    }

    memset(icmph, 0, packet_size);
    icmph->type = ICMP_ECHO_REQUEST;
    icmph->code = 0;
    icmph->data.echo.identifier = htons(id);
    icmph->data.echo.sequence_number = htons(seq);

    if (data && data_len > 0) {
        memcpy((char*)icmph + sizeof(struct icmp_header), data, data_len);
    }

    icmph->checksum = 0;
    icmph->checksum = icmp_calculate_checksum(icmph, packet_size);

    printf("[ICMP] Sending Echo Request to %x (ID: %d, Seq: %d)\n", dest_ip, id, seq);

    int result = ip_send_packet(dest_ip, IP_PROTO_ICMP, icmph, packet_size);
    kfree(icmph);
    return result;
}

int icmp_send_echo_reply(uint32_t dest_ip, uint16_t id, uint16_t seq, void* data, size_t data_len) {
    size_t packet_size = sizeof(struct icmp_header) + data_len;
    struct icmp_header* icmph = (struct icmp_header*)kmalloc(packet_size);

    if (!icmph) {
        printf("[ICMP] Failed to allocate memory for echo reply\n");
        return -1;
    }

    memset(icmph, 0, packet_size);
    icmph->type = ICMP_ECHO_REPLY;
    icmph->code = 0;
    icmph->data.echo.identifier = htons(id);
    icmph->data.echo.sequence_number = htons(seq);

    if (data && data_len > 0) {
        memcpy((char*)icmph + sizeof(struct icmp_header), data, data_len);
    }

    icmph->checksum = 0;
    icmph->checksum = icmp_calculate_checksum(icmph, packet_size);

    int result = ip_send_packet(dest_ip, IP_PROTO_ICMP, icmph, packet_size);
    kfree(icmph);
    return result;
}

int icmp_send_dest_unreachable(uint32_t dest_ip, uint8_t code, struct ip_header* original_ip, void* original_data, size_t original_data_len) {
    size_t original_data_copy_len = (original_data_len > 8) ? 8 : original_data_len;
    size_t packet_size = sizeof(struct icmp_header) + sizeof(struct ip_header) + original_data_copy_len;
    struct icmp_header* icmph = (struct icmp_header*)kmalloc(packet_size);

    if (!icmph) {
        printf("[ICMP] Failed to allocate memory for dest unreachable\n");
        return -1;
    }

    memset(icmph, 0, packet_size);
    icmph->type = ICMP_DEST_UNREACHABLE;
    icmph->code = code;

    struct icmp_error_message* error_msg = (struct icmp_error_message*)((char*)icmph + sizeof(struct icmp_header));
    memcpy(&error_msg->original_ip_header, original_ip, sizeof(struct ip_header));
    if (original_data && original_data_copy_len > 0) {
        memcpy(error_msg->original_data, original_data, original_data_copy_len);
    }

    icmph->checksum = 0;
    icmph->checksum = icmp_calculate_checksum(icmph, packet_size);

    int result = ip_send_packet(dest_ip, IP_PROTO_ICMP, icmph, packet_size);
    kfree(icmph);
    return result;
}

int icmp_send_time_exceeded(uint32_t dest_ip, uint8_t code, struct ip_header* original_ip, void* original_data, size_t original_data_len) {
    size_t original_data_copy_len = (original_data_len > 8) ? 8 : original_data_len;
    size_t packet_size = sizeof(struct icmp_header) + sizeof(struct ip_header) + original_data_copy_len;
    struct icmp_header* icmph = (struct icmp_header*)kmalloc(packet_size);

    if (!icmph) {
        printf("[ICMP] Failed to allocate memory for time exceeded\n");
        return -1;
    }

    memset(icmph, 0, packet_size);
    icmph->type = ICMP_TIME_EXCEEDED;
    icmph->code = code;

    struct icmp_error_message* error_msg = (struct icmp_error_message*)((char*)icmph + sizeof(struct icmp_header));
    memcpy(&error_msg->original_ip_header, original_ip, sizeof(struct ip_header));
    if (original_data && original_data_copy_len > 0) {
        memcpy(error_msg->original_data, original_data, original_data_copy_len);
    }

    icmph->checksum = 0;
    icmph->checksum = icmp_calculate_checksum(icmph, packet_size);

    int result = ip_send_packet(dest_ip, IP_PROTO_ICMP, icmph, packet_size);
    kfree(icmph);
    return result;
}

void icmp_handle_packet(struct ip_header* iph) {
    uint8_t ip_header_len = (iph->version_ihl & 0x0F) * 4;
    struct icmp_header* icmph = (struct icmp_header*)((char*)iph + ip_header_len);
    size_t total_icmp_len = ntohs(iph->total_len) - ip_header_len;

    uint16_t received_checksum = icmph->checksum;
    icmph->checksum = 0;
    uint16_t calculated_checksum = icmp_calculate_checksum(icmph, total_icmp_len);
    icmph->checksum = received_checksum;

    if (calculated_checksum != 0) {
        printf("[ICMP] Checksum error in received packet\n");
        return;
    }

    uint32_t src_ip = ntohl(iph->src_ip);
    uint32_t dst_ip = ntohl(iph->dst_ip);

    printf("[ICMP] Received packet - Type: %d, Code: %d, From: %x, To: %x\n", 
           icmph->type, icmph->code, src_ip, dst_ip);

    switch (icmph->type) {
        case ICMP_ECHO_REQUEST:
            printf("[ICMP] Processing Echo Request from %x\n", src_ip);
            {
                size_t data_len = total_icmp_len - sizeof(struct icmp_header);
                char* data = (char*)icmph + sizeof(struct icmp_header);
                
                icmp_send_echo_reply(src_ip, 
                                   ntohs(icmph->data.echo.identifier), 
                                   ntohs(icmph->data.echo.sequence_number), 
                                   data, 
                                   data_len);
            }
            break;
        case ICMP_ECHO_REPLY:
            printf("[ICMP] Echo Reply received - ID: %d, Seq: %d\n", 
                   ntohs(icmph->data.echo.identifier), 
                   ntohs(icmph->data.echo.sequence_number));
            break;
        case ICMP_DEST_UNREACHABLE:
            printf("[ICMP] Destination Unreachable received - Code: %d\n", icmph->code);
            break;
        case ICMP_TIME_EXCEEDED:
            printf("[ICMP] Time Exceeded received - Code: %d\n", icmph->code);
            break;
        default:
            printf("[ICMP] Unknown ICMP type %d\n", icmph->type);
            break;
    }
}

