#ifndef DNS_H
#define DNS_H

#include "types.h"
#include "socket.h"
#include "lib/string.h"

#define DNS_PORT 53
#define DNS_MAX_PACKET_SIZE 512
#define DNS_MAX_NAME_SIZE 256
#define DNS_MAX_QUESTIONS 10
#define DNS_MAX_ANSWERS 10

// DNS Header Flags
#define DNS_FLAGS_QR_QUERY 0x0000
#define DNS_FLAGS_QR_RESPONSE 0x8000
#define DNS_FLAGS_OPCODE_QUERY 0x0000
#define DNS_FLAGS_AA_AUTHORITATIVE 0x0400
#define DNS_FLAGS_TC_TRUNCATED 0x0200
#define DNS_FLAGS_RD_RECURSION_DESIRED 0x0100
#define DNS_FLAGS_RA_RECURSION_AVAILABLE 0x8000

// DNS Resource Record Types
#define DNS_TYPE_A 1          // IPv4 Address
#define DNS_TYPE_NS 2         // Name Server
#define DNS_TYPE_CNAME 5      // Canonical Name
#define DNS_TYPE_SOA 6        // Start of Authority
#define DNS_TYPE_PTR 12       // Pointer
#define DNS_TYPE_MX 15        // Mail Exchange
#define DNS_TYPE_TXT 16       // Text
#define DNS_TYPE_AAAA 28      // IPv6 Address

// DNS Classes
#define DNS_CLASS_IN 1        // Internet

// DNS Query Types
#define DNS_QUERY_TYPE_A 1
#define DNS_QUERY_TYPE_PTR 12

// DNS Response Codes
#define DNS_RCODE_NO_ERROR 0
#define DNS_RCODE_FORMAT_ERROR 1
#define DNS_RCODE_SERVER_FAILURE 2
#define DNS_RCODE_NAME_ERROR 3
#define DNS_RCODE_NOT_IMPLEMENTED 4
#define DNS_RCODE_REFUSED 5

struct dns_header {
    uint16_t id;              // Transaction ID
    uint16_t flags;           // Flags
    uint16_t qdcount;         // Number of questions
    uint16_t ancount;         // Number of answers
    uint16_t nscount;         // Number of authority records
    uint16_t arcount;         // Number of additional records
} __attribute__((packed));

struct dns_question {
    uint16_t qtype;
    uint16_t qclass;
} __attribute__((packed));

struct dns_resource_record {
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
    uint8_t* rdata;
} __attribute__((packed));

struct dns_answer {
    char name[DNS_MAX_NAME_SIZE];
    struct dns_resource_record rr;
};

struct dns_cache_entry {
    char hostname[DNS_MAX_NAME_SIZE];
    uint32_t ip_address;
    uint32_t timestamp;
    uint8_t valid;
};

// DNS Resolver structure
struct dns_resolver {
    uint16_t next_transaction_id;
    uint8_t initialized;
    struct dns_cache_entry cache[128]; // DNS cache
    uint32_t dns_server_ip; // Primary DNS server
    uint32_t secondary_dns_server_ip; // Secondary DNS server
};

// DNS API functions
void dns_init(void);
uint32_t dns_resolve(const char* hostname);
int dns_resolve_async(const char* hostname, void (*callback)(uint32_t ip, void* context), void* context);
int dns_reverse_lookup(uint32_t ip, char* hostname, size_t hostname_len);
int dns_cache_add(const char* hostname, uint32_t ip);
uint32_t dns_cache_lookup(const char* hostname);
void dns_flush_cache(void);
int dns_set_nameserver(uint32_t primary_dns, uint32_t secondary_dns);
int dns_parse_name(const uint8_t* packet, size_t packet_len, size_t* pos, char* name, size_t name_len);
int dns_encode_name(const char* hostname, uint8_t* buffer, size_t* len);

#endif
