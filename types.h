#ifndef TYPES_H
#define TYPES_H

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef long long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef char int8_t;
typedef uint64_t size_t;
typedef uint64_t vaddr_t;
typedef uint64_t paddr_t;
typedef int pid_t;
typedef int uid_t;
typedef int gid_t;
typedef int mode_t;
typedef long off_t;
typedef long ssize_t;
typedef unsigned long nfds_t;
typedef unsigned long socklen_t;
typedef unsigned long time_t;
typedef unsigned long clock_t;
typedef unsigned long long key_t;
typedef unsigned long long key_serial_t;
typedef int mqd_t;
typedef void* timer_t;
typedef int clockid_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned long nlink_t;
typedef unsigned long blksize_t;
typedef unsigned long blkcnt_t;

// Добавлено для сетевых функций
typedef enum { false = 0, true = 1 } bool;

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
};

// Константы для сокетов
#define AF_INET         2
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_ICMP    1

// Структура для sockaddr_in
struct in_addr {
    uint32_t s_addr;
};

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    struct in_addr sin_addr;
    uint8_t sin_zero[8];
};

#endif

