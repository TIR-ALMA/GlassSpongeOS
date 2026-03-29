#ifndef DNS_UTILS_H
#define DNS_UTILS_H

#include "dns.h"

// Utility functions for DNS
char* dns_format_ip(uint32_t ip, char* buffer, size_t buffer_len);
int dns_is_valid_hostname(const char* hostname);
int dns_extract_domain_from_url(const char* url, char* domain, size_t domain_len);
uint32_t dns_string_to_ip(const char* ip_str);
int dns_ip_to_string(uint32_t ip, char* str, size_t str_len);

#endif
