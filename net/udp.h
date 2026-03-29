#ifndef UDP_H
#define UDP_H

#include "types.h"
#include "ip.h"
#include "lib/list.h"  // Для очереди пакетов

#define UDP_PORT_ANY 0
#define UDP_MAX_PACKET_SIZE 65535
#define UDP_DEFAULT_BUFFER_SIZE 4096

// UDP заголовок (RFC 768)
struct udp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;      // длина заголовка + данных (в байтах)
    uint16_t checksum; // 0 = нет проверки (для IPv4 разрешено)
} __attribute__((packed));

// Структура UDP-сокета
struct udp_socket {
    uint16_t local_port;
    uint32_t local_ip;       // 0.0.0.0 = любой интерфейс
    uint16_t remote_port;    // 0 = не привязан к удалённому хосту
    uint32_t remote_ip;      // 0.0.0.0 = любой удалённый хост
    bool bound;
    bool connected;

    // Буфер приёма (циклический)
    uint8_t* recv_buf;
    size_t recv_buf_size;
    size_t recv_head;   // указатель на начало данных
    size_t recv_tail;   // указатель на конец данных (следующее место для записи)
    size_t recv_count;  // сколько байт в буфере

    // Очередь входящих пакетов (для многопоточности)
    struct list_head packet_queue;
    spinlock_t lock;

    // Связь с процессом
    struct process* owner;
    wait_queue_t recv_wait;

    struct list_node node; // для глобального списка сокетов
};

// Входящий UDP-пакет (хранится в очереди)
struct udp_packet {
    struct list_node node;
    uint32_t src_ip;
    uint16_t src_port;
    uint16_t dst_port;
    size_t data_len;
    uint8_t data[0];  // flexible array member
};

// Глобальный список всех UDP-сокетов
extern struct list_head udp_sockets_list;
extern spinlock_t udp_sockets_lock;

// Инициализация
void udp_init(void);

// Сокетные функции
int udp_socket(void);
int udp_bind(int sockfd, const struct sockaddr_in* addr);
int udp_connect(int sockfd, const struct sockaddr_in* addr);
int udp_sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr_in* dest_addr);
int udp_recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr_in* src_addr, socklen_t* addrlen);
int udp_close(int sockfd);

// Обработка входящего пакета (вызывается из ip_handle_packet)
void udp_handle_packet(struct ip_header* iph, uint8_t* payload, size_t payload_len);

// Вспомогательные
struct udp_socket* udp_get_socket_by_port(uint16_t port);
struct udp_socket* udp_get_socket_by_fd(int sockfd);
bool udp_is_port_free(uint16_t port);

#endif
