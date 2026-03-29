#include "dns_utils.h"
#include "lib/string.h"
#include "lib/printf.h"

char* dns_format_ip(uint32_t ip, char* buffer, size_t buffer_len) {
    if(!buffer || buffer_len < 16) return NULL;
    
    snprintf(buffer, buffer_len, "%d.%d.%d.%d",
             (uint8_t)(ip & 0xFF),
             (uint8_t)((ip >> 8) & 0xFF),
             (uint8_t)((ip >> 16) & 0xFF),
             (uint8_t)((ip >> 24) & 0xFF));
    
    return buffer;
}

int dns_is_valid_hostname(const char* hostname) {
    if(!hostname) return 0;
    
    size_t len = strlen(hostname);
    if(len == 0 || len > 253) return 0;
    
    if(hostname[0] == '.' || hostname[len-1] == '.') return 0;
    
    const char* ptr = hostname;
    int label_len = 0;
    
    while(*ptr) {
        if(*ptr == '.') {
            if(label_len == 0 || label_len > 63) return 0;
            label_len = 0;
        } else if((*ptr >= 'a' && *ptr <= 'z') ||
                  (*ptr >= 'A' && *ptr <= 'Z') ||
                  (*ptr >= '0' && *ptr <= '9') ||
                  *ptr == '-') {
            label_len++;
            if(label_len > 63) return 0;
        } else {
            return 0;
        }
        ptr++;
    }
    
    return (label_len > 0 && label_len <= 63) ? 1 : 0;
}

int dns_extract_domain_from_url(const char* url, char* domain, size_t domain_len) {
    if(!url || !domain || domain_len == 0) return -1;
    
    // Skip protocol (http://, https://, etc.)
    const char* start = url;
    const char* proto_end = strstr(url, "://");
    if(proto_end) {
        start = proto_end + 3;
    }
    
    // Find end of domain (port, path, query, etc.)
    const char* end = start;
    while(*end && *end != ':' && *end != '/' && *end != '?' && *end != '#') {
        end++;
    }
    
    size_t domain_length = end - start;
    if(domain_length >= domain_len) return -1;
    
    strncpy(domain, start, domain_length);
    domain[domain_length] = '\0';
    
    return 0;
}

uint32_t dns_string_to_ip(const char* ip_str) {
    if(!ip_str) return 0;
    
    uint32_t ip = 0;
    int parts[4] = {0, 0, 0, 0};
    int part = 0;
    const char* ptr = ip_str;
    
    while(*ptr && part < 4) {
        if(*ptr == '.') {
            part++;
            ptr++;
        } else if(*ptr >= '0' && *ptr <= '9') {
            parts[part] = parts[part] * 10 + (*ptr - '0');
            ptr++;
        } else {
            return 0; // Invalid character
        }
    }
    
    if(part != 3) return 0; // Not enough parts
    
    // Validate each part is 0-255
    for(int i = 0; i < 4; i++) {
        if(parts[i] > 255) return 0;
    }
    
    ip = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return htonl(ip);
}

int dns_ip_to_string(uint32_t ip, char* str, size_t str_len) {
    if(!str || str_len < 16) return -1;
    
    uint32_t net_ip = ntohl(ip);
    snprintf(str, str_len, "%d.%d.%d.%d",
             (net_ip >> 24) & 0xFF,
             (net_ip >> 16) & 0xFF,
             (net_ip >> 8) & 0xFF,
             net_ip & 0xFF);
    
    return 0;
}
