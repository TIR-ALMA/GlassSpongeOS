#include "dns.h"
#include "udp.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "mm.h"
#include "drivers/timer.h"

static struct dns_resolver resolver;

// DNS magic cookie
static const uint32_t dns_magic_cookie = 0x00010100; // Standard DNS magic

void dns_init(void) {
    if(resolver.initialized) return;
    
    memset(&resolver, 0, sizeof(struct dns_resolver));
    
    // Initialize with Google's public DNS servers
    resolver.dns_server_ip = htonl(0x08080808); // 8.8.8.8
    resolver.secondary_dns_server_ip = htonl(0x08080404); // 8.8.4.4
    resolver.next_transaction_id = 1;
    resolver.initialized = 1;
    
    // Initialize cache
    for(int i = 0; i < 128; i++) {
        resolver.cache[i].valid = 0;
        resolver.cache[i].hostname[0] = '\0';
    }
    
    printf("DNS resolver initialized\n");
}

// Encode hostname in DNS format (length-prefixed labels)
int dns_encode_name(const char* hostname, uint8_t* buffer, size_t* len) {
    if(!hostname || !buffer || !len) return -1;
    
    size_t pos = 0;
    const char* start = hostname;
    const char* dot = start;
    
    while(*dot) {
        // Find next dot
        while(*dot && *dot != '.') dot++;
        
        size_t label_len = dot - start;
        if(label_len > 63) return -1; // Label too long
        
        buffer[pos++] = (uint8_t)label_len;
        memcpy(buffer + pos, start, label_len);
        pos += label_len;
        
        if(*dot == '.') {
            dot++;
            start = dot;
        } else {
            break;
        }
    }
    
    buffer[pos++] = 0; // Root label (end of name)
    *len = pos;
    return 0;
}

// Parse compressed name from DNS packet
int dns_parse_name(const uint8_t* packet, size_t packet_len, size_t* pos, char* name, size_t name_len) {
    if(!packet || !pos || !name || name_len == 0) return -1;
    
    size_t current_pos = *pos;
    size_t name_pos = 0;
    int jumped = 0;
    size_t jump_pos = 0;
    
    while(current_pos < packet_len) {
        uint8_t label_len = packet[current_pos];
        
        if(label_len == 0) {
            // End of name
            name[name_pos] = '\0';
            if(!jumped) *pos = current_pos + 1;
            return 0;
        } else if((label_len & 0xC0) == 0xC0) {
            // Compression pointer
            if(current_pos + 1 >= packet_len) return -1;
            
            if(!jumped) {
                jump_pos = current_pos + 2;
                jumped = 1;
            }
            
            current_pos = ((label_len & 0x3F) << 8) | packet[current_pos + 1];
            if(current_pos >= packet_len) return -1;
            continue;
        } else {
            // Regular label
            if(name_pos + label_len + 1 >= name_len) return -1; // Name too long
            
            if(current_pos + 1 + label_len > packet_len) return -1;
            
            if(name_pos > 0) {
                name[name_pos++] = '.';
            }
            
            memcpy(name + name_pos, packet + current_pos + 1, label_len);
            name_pos += label_len;
            current_pos += 1 + label_len;
        }
    }
    
    if(jumped) {
        *pos = jump_pos;
    }
    
    name[name_pos] = '\0';
    return 0;
}

// Build DNS query packet
static int build_dns_query(const char* hostname, uint8_t* packet, size_t* packet_len, uint16_t* transaction_id) {
    if(!hostname || !packet || !packet_len) return -1;
    
    struct dns_header* header = (struct dns_header*)packet;
    
    // Generate transaction ID
    *transaction_id = resolver.next_transaction_id++;
    if(resolver.next_transaction_id == 0) resolver.next_transaction_id = 1;
    
    header->id = htons(*transaction_id);
    header->flags = htons(DNS_FLAGS_QR_QUERY | DNS_FLAGS_RD_RECURSION_DESIRED);
    header->qdcount = htons(1);  // One question
    header->ancount = htons(0);  // No answers yet
    header->nscount = htons(0);  // No authority records
    header->arcount = htons(0);  // No additional records
    
    size_t pos = sizeof(struct dns_header);
    
    // Encode hostname
    size_t name_len;
    if(dns_encode_name(hostname, packet + pos, &name_len) < 0) return -1;
    pos += name_len;
    
    // Add question type and class
    struct dns_question* question = (struct dns pos);
    question->qtype = htons(DNS_QUERY_TYPE_A);
    question->qclass = htons(DNS_CLASS_IN);
    pos += sizeof(struct dns_question);
    
    *packet_len = pos;
    return 0;
}

// Parse DNS response
static int parse_dns_response(const uint8_t* packet, size_t packet_len, uint32_t* ip_address) {
    if(!packet || packet_len < sizeof(struct dns_header) || !ip_address) return -1;
    
    struct dns_header* header = (struct dns_header*)packet;
    
    // Check if it's a response
    if((ntohs(header->flags) & DNS_FLAGS_QR_RESPONSE) == 0) {
        printf("DNS: Not a response packet\n");
        return -1;
    }
    
    // Check response code
    uint8_t rcode = ntohs(header->flags) & 0x000F;
    if(rcode != DNS_RCODE_NO_ERROR) {
        printf("DNS: Response error code: %d\n", rcode);
        return -1;
    }
    
    // Check if there are answers
    uint16_t ancount = ntohs(header->ancount);
    if(ancount == 0) {
        printf("DNS: No answers in response\n");
        return -1;
    }
    
    size_t pos = sizeof(struct dns_header);
    
    // Skip questions
    uint16_t qdcount = ntohs(header->qdcount);
    for(int i = 0; i < qdcount; i++) {
        // Skip name
        while(pos < packet_len && packet[pos] != 0) {
            if((packet[pos] & 0xC0) == 0xC0) {
                pos += 2;
                break;
            } else {
                pos += 1 + packet[pos];
            }
        }
        pos++; // Skip null terminator
        
        // Skip qtype and qclass
        pos += sizeof(struct dns_question) - 2; // Minus the 2 bytes we didn't skip for the name
    }
    
    // Parse answers
    for(int i = 0; i < ancount; i++) {
        char name[DNS_MAX_NAME_SIZE];
        if(dns_parse_name(packet, packet_len, &pos, name, sizeof(name)) < 0) {
            printf("DNS: Error parsing answer name\n");
            return -1;
        }
        
        if(pos + sizeof(uint16_t) * 2 + sizeof(uint32_t) + sizeof(uint16_t) > packet_len) {
            printf("DNS: Packet too short for answer\n");
            return -1;
        }
        
        uint16_t type = ntohs(*(uint16_t*)(packet + pos)); pos += 2;
        uint16_t class = ntohs(*(uint16_t*)(packet + pos)); pos += 2;
        uint32_t ttl = ntohl(*(uint32_t*)(packet + pos)); pos += 4;
        uint16_t rdlength = ntohs(*(uint16_t*)(packet + pos)); pos += 2;
        
        if(pos + rdlength > packet_len) {
            printf("DNS: Invalid resource data length\n");
            return -1;
        }
        
        if(type == DNS_TYPE_A && class == DNS_CLASS_IN && rdlength == 4) {
            // Found IPv4 address
            *ip_address = *(uint32_t*)(packet + pos);
            return 0; // Success
        }
        
        pos += rdlength;
    }
    
    printf("DNS: No A record found in answers\n");
    return -1;
}

// Send DNS query and wait for response
static int send_dns_query(const char* hostname, uint32_t dns_server_ip, uint32_t* ip_address) {
    if(!hostname || !ip_address) return -1;
    
    uint8_t query_packet[DNS_MAX_PACKET_SIZE];
    size_t query_len;
    uint16_t transaction_id;
    
    if(build_dns_query(hostname, query_packet, &query_len, &transaction_id) < 0) {
        printf("DNS: Failed to build query packet\n");
        return -1;
    }
    
    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock < 0) {
        printf("DNS: Failed to create UDP socket\n");
        return -1;
    }
    
    // Set up DNS server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DNS_PORT);
    server_addr.sin_addr.s_addr = dns_server_ip;
    
    // Send query
    ssize_t sent = sendto(sock, query_packet, query_len, 0, 
                         (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(sent < 0) {
        printf("DNS: Failed to send query\n");
        close(sock);
        return -1;
    }
    
    // Receive response
    uint8_t response_packet[DNS_MAX_PACKET_SIZE];
    struct sockaddr_in response_addr;
    socklen_t addr_len = sizeof(response_addr);
    
    // Set timeout (simplified - would use select/poll in real implementation)
    ssize_t received = recvfrom(sock, response_packet, sizeof(response_packet), 0,
                               (struct sockaddr*)&response_addr, &addr_len);
    
    if(received < 0) {
        printf("DNS: Failed to receive response\n");
        close(sock);
        return -1;
    }
    
    // Verify response comes from expected server and has correct transaction ID
    if(response_addr.sin_addr.s_addr != dns_server_ip) {
        printf("DNS: Response from unexpected server\n");
        close(sock);
        return -1;
    }
    
    struct dns_header* response_header = (struct dns_header*)response_packet;
    if(ntohs(response_header->id) != transaction_id) {
        printf("DNS: Transaction ID mismatch\n");
        close(sock);
        return -1;
    }
    
    // Parse response
    int result = parse_dns_response(response_packet, received, ip_address);
    
    close(sock);
    return result;
}

// Main DNS resolve function
uint32_t dns_resolve(const char* hostname) {
    if(!resolver.initialized) {
        dns_init();
    }
    
    if(!hostname) return 0;
    
    // First check cache
    uint32_t cached_ip = dns_cache_lookup(hostname);
    if(cached_ip != 0) {
        printf("DNS: Found %s in cache: %d.%d.%d.%d\n", 
               hostname,
               (uint8_t)(cached_ip & 0xFF),
               (uint8_t)((cached_ip >> 8) & 0xFF),
               (uint8_t)((cached_ip >> 16) & 0xFF),
               (uint8_t)((cached_ip >> 24) & 0xFF));
        return cached_ip;
    }
    
    printf("DNS: Resolving %s\n", hostname);
    
    uint32_t ip_address = 0;
    
    // Try primary DNS server
    if(send_dns_query(hostname, resolver.dns_server_ip, &ip_address) == 0) {
        // Add to cache
        dns_cache_add(hostname, ip_address);
        return ip_address;
    }
    
    // Try secondary DNS server
    if(resolver.secondary_dns_server_ip != 0) {
        if(send_dns_query(hostname, resolver.secondary_dns_server_ip, &ip_address) == 0) {
            // Add to cache
            dns_cache_add(hostname, ip_address);
            return ip_address;
        }
    }
    
    printf("DNS: Failed to resolve %s\n", hostname);
    return 0;
}

// Async DNS resolve (placeholder implementation)
int dns_resolve_async(const char* hostname, void (*callback)(uint32_t ip, void* context), void* context) {
    // In a real implementation, this would use threading or callbacks
    // For now, we'll do it synchronously
    if(!hostname || !callback) return -1;
    
    uint32_t ip = dns_resolve(hostname);
    callback(ip, context);
    return 0;
}

// Reverse DNS lookup (PTR record)
int dns_reverse_lookup(uint32_t ip, char* hostname, size_t hostname_len) {
    if(!hostname || hostname_len == 0) return -1;
    
    // Convert IP to reverse lookup format (e.g., 1.2.3.4 -> 4.3.2.1.in-addr.arpa)
    snprintf(hostname, hostname_len, "%d.%d.%d.%d.in-addr.arpa",
             (uint8_t)(ip >> 24), (uint8_t)((ip >> 16) & 0xFF),
             (uint8_t)((ip >> 8) & 0xFF), (uint8_t)(ip & 0xFF));
    
    // In a real implementation, you'd send a PTR query
    // This is a placeholder
    printf("DNS: Reverse lookup not fully implemented for %d.%d.%d.%d\n",
           (uint8_t)(ip & 0xFF), (uint8_t)((ip >> 8) & 0xFF),
           (uint8_t)((ip >> 16) & 0xFF), (uint8_t)(ip >> 24));
    
    return -1;
}

// DNS cache functions
int dns_cache_add(const char* hostname, uint32_t ip) {
    if(!resolver.initialized || !hostname) return -1;
    
    // Find empty slot or oldest entry
    int oldest_idx = -1;
    uint32_t oldest_time = UINT32_MAX;
    
    for(int i = 0; i < 128; i++) {
        if(!resolver.cache[i].valid) {
            oldest_idx = i;
            break;
        }
        if(resolver.cache[i].timestamp < oldest_time) {
            oldest_idx = i;
            oldest_time = resolver.cache[i].timestamp;
        }
    }
    
    if(oldest_idx == -1) return -1; // Cache full
    
    strncpy(resolver.cache[oldest_idx].hostname, hostname, DNS_MAX_NAME_SIZE - 1);
    resolver.cache[oldest_idx].hostname[DNS_MAX_NAME_SIZE - 1] = '\0';
    resolver.cache[oldest_idx].ip_address = ip;
    resolver.cache[oldest_idx].timestamp = timer_get_ticks();
    resolver.cache[oldest_idx].valid = 1;
    
    return 0;
}

uint32_t dns_cache_lookup(const char* hostname) {
    if(!resolver.initialized || !hostname) return 0;
    
    for(int i = 0; i < 128; i++) {
        if(resolver.cache[i].valid && 
           strcmp(resolver.cache[i].hostname, hostname) == 0) {
            return resolver.cache[i].ip_address;
        }
    }
    
    return 0;
}

void dns_flush_cache(void) {
    if(!resolver.initialized) return;
    
    for(int i = 0; i < 128; i++) {
        resolver.cache[i].valid = 0;
        resolver.cache[i].hostname[0] = '\0';
    }
}

int dns_set_nameserver(uint32_t primary_dns, uint32_t secondary_dns) {
    if(!resolver.initialized) return -1;
    
    resolver.dns_server_ip = primary_dns;
    resolver.secondary_dns_server_ip = secondary_dns;
    return 0;
}
