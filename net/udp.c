#include "udp.h"
#include "ip.h"
#include "arp.h"
#include "ethernet.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "lib/mm.h"
#include "lib/list.h"
#include "drivers/timer.h"
#include "sched.h"

// Глобальные данные
struct list_head udp_sockets_list = LIST_HEAD_INIT(udp_sockets_list);
spinlock_t udp_sockets_lock = SPINLOCK_INIT;

// --- Вспомогательные функции ---

static inline bool is_broadcast_ip(uint32_t ip) {
    return ip == 0xFFFFFFFF || (ip & 0xFFFFFF00) == 0xC0A801FF; // 192.168.1.255
}

static inline bool ip_match(uint32_t a, uint32_t b, uint32_t mask) {
    return (a & mask) == (b & mask);
}

// Поиск сокета по порту (для входящих пакетов)
struct udp_socket* udp_get_socket_by_port(uint16_t port) {
    spin_lock(&udp_sockets_lock);
    struct list_node* n;
    list_for_each(n, &udp_sockets_list) {
        struct udp_socket* s = container_of(n, struct udp_socket, node);
        if (s->bound && s->local_port == port) {
            spin_unlock(&udp_sockets_lock);
            return s;
        }
    }
    spin_unlock(&udp_sockets_lock);
    return NULL;
}

// Поиск сокета по fd
struct udp_socket* udp_get_socket_by_fd(int sockfd) {
    if (sockfd < 0) return NULL;
    // В реальной ОС: сокеты хранятся в таблице процесса
    // Здесь упрощённо — предположим, что sockfd == указатель
    return (struct udp_socket*)(uintptr_t)sockfd;
}

bool udp_is_port_free(uint16_t port) {
    spin_lock(&udp_sockets_lock);
    struct list_node* n;
    list_for_each(n, &udp_sockets_list) {
        struct udp_socket* s = container_of(n, struct udp_socket, node);
        if (s->bound && s->local_port == port) {
            spin_unlock(&udp_sockets_lock);
            return false;
        }
    }
    spin_unlock(&udp_sockets_lock);
    return true;
}

// --- Инициализация ---

void udp_init(void) {
    printf("[UDP] Initializing...\n");
    // Ничего особенного — только глобальные списки уже инициализированы
}

// --- Создание сокета ---

int udp_socket(void) {
    struct udp_socket* sock = (struct udp_socket*)kmalloc(sizeof(struct udp_socket));
    if (!sock) return -1;

    memset(sock, 0, sizeof(*sock));
    sock->recv_buf_size = UDP_DEFAULT_BUFFER_SIZE;
    sock->recv_buf = (uint8_t*)kmalloc(sock->recv_buf_size);
    if (!sock->recv_buf) {
        kfree(sock);
        return -1;
    }

    INIT_LIST_HEAD(&sock->packet_queue);
    spin_lock_init(&sock->lock);
    init_wait_queue(&sock->recv_wait);

    // Добавить в глобальный список
    spin_lock(&udp_sockets_lock);
    list_add_tail(&sock->node, &udp_sockets_list);
    spin_unlock(&udp_sockets_lock);

    // Вернуть указатель как fd (в реальной ОС — индекс в таблице)
    return (int)(uintptr_t)sock;
}

// --- Привязка к адресу ---

int udp_bind(int sockfd, const struct sockaddr_in* addr) {
    struct udp_socket* sock = udp_get_socket_by_fd(sockfd);
    if (!sock) return -1;

    spin_lock(&sock->lock);

    if (sock->bound) {
        spin_unlock(&sock->lock);
        return -1; // Уже привязан
    }

    uint16_t port = ntohs(addr->sin_port);
    uint32_t ip = addr->sin_addr.s_addr;

    // Проверка: порт занят?
    if (port != 0 && !udp_is_port_free(port)) {
        spin_unlock(&sock->lock);
        return -1;
    }

    sock->local_port = port;
    sock->local_ip = ip;
    sock->bound = true;

    spin_unlock(&sock->lock);
    return 0;
}

// --- Подключение к удалённому хосту ---

int udp_connect(int sockfd, const struct sockaddr_in* addr) {
    struct udp_socket* sock = udp_get_socket_by_fd(sockfd);
    if (!sock) return -1;

    spin_lock(&sock->lock);
    sock->remote_port = ntohs(addr->sin_port);
    sock->remote_ip = addr->sin_addr.s_addr;
    sock->connected = true;
    spin_unlock(&sock->lock);
    return 0;
}

// --- Отправка пакета ---

int udp_sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr_in* dest_addr) {
    if (len > UDP_MAX_PACKET_SIZE - sizeof(struct udp_header)) {
        return -1;
    }

    struct udp_socket* sock = udp_get_socket_by_fd(sockfd);
    if (!sock || !sock->bound) return -1;

    // Определить целевой адрес
    uint32_t dst_ip;
    uint16_t dst_port;

    if (dest_addr) {
        dst_ip = dest_addr->sin_addr.s_addr;
        dst_port = ntohs(dest_addr->sin_port);
    } else if (sock->connected) {
        dst_ip = sock->remote_ip;
        dst_port = sock->remote_port;
    } else {
        return -1; // Не указан адрес и не connected
    }

    // Разрешить отправку на broadcast?
    if (is_broadcast_ip(dst_ip) && !(flags & MSG_DONTROUTE)) {
        // В реальной ОС нужно проверить флаг SO_BROADCAST
    }

    // Формируем UDP-пакет
    size_t total_len = sizeof(struct udp_header) + len;
    uint8_t* packet = (uint8_t*)kmalloc(total_len);
    if (!packet) return -1;

    struct udp_header* udph = (struct udp_header*)packet;
    udph->src_port = htons(sock->local_port);
    udph->dst_port = htons(dst_port);
    udph->len = htons(total_len);
    udph->checksum = 0; // IPv4 разрешает 0

    memcpy(packet + sizeof(struct udp_header), buf, len);

 // Вычисляем чек-сумму (если нужна)
    // В IPv4 чек-сумма опциональна, но мы её считаем для совместимости
    udph->checksum = udp_checksum(
        packet, total_len,
        sock->local_ip, dst_ip,
        total_len
    );

    // Отправляем через IP
    int ret = ip_send(dst_ip IP_PROTO_UDP, packet, total_len);
    kfree(packet);

    return ret >= 0 ? len : -1;
}

// --- Приём пакета ---

int udp_recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr_in* src_addr, socklen_t* addrlen) {
    struct udp_socket* sock = udp_get_socket_by_fd(sockfd);
    if (!sock) return -1;

    // Блокировка до получения данных (если нет данных и не O_NONBLOCK)
    if (!(flags & MSG_DONTWAIT)) {
        wait_event_interruptible(sock->recv_wait, sock->recv_count > 0);
        if (current_process->state == PROC_INTERRUPTED) {
            return -1;
        }
    }

    spin_lock(&sock->lock);

    if (sock->recv_count == 0) {
        spin_unlock(&sock->lock);
        return -1; // EAGAIN/EWOULDBLOCK
    }

    // Получаем первый пакет из очереди
    struct list_node* head = sock->packet_queue.next;
    struct udp_packet* pkt = container_of(head, struct udp_packet, node);
    list_del(head);

    // Копируем данные
    size_t copy_len = (len < pkt->data_len) ? len : pkt->data_len;
    memcpy(buf, pkt->data, copy_len);

    // Заполняем src_addr
    if (src_addr && addrlen) {
        src_addr->sin_family = AF_INET;
        src_addr->sin_port = htons(pkt->src_port);
        src_addr->sin_addr.s_addr = pkt->src_ip;
        *addrlen = sizeof(struct sockaddr_in);
    }

    // Освобождаем пакет
    kfree(pkt);

    sock->recv_count -= copy_len;
    spin_unlock(&sock->lock);

    return copy_len;
}

// --- Закрытие сокета ---

int udp_close(int sockfd) {
    struct udp_socket* sock = udp_get_by_fd(sockfd);
    if (!sock) return -1;

    spin_lock(&udp_sockets_lock);
    list_del(&sock->node);
    spin_unlock(&udp_sockets_lock);

    // Очистка очереди пакетов
    struct list_node* n, *tmp;
    list_for_each_safe(n, tmp, &sock->packet_queue) {
        struct udp_packet* pkt = container_of(n, struct udp_packet, node);
        kfree(pkt);
    }

    kfree(sock->recv_buf);
    kfree(sock);
    return 0;
}

// --- Обработка входящего UDP-пакета ---

void udp_handle_packet(struct ip_header* iph, uint8_t* payload, size_t payload_len) {
    if (payload_len < sizeof(struct udp_header)) return;

    struct udp_header* udph = (struct udp_header*)payload;
    uint16_t src_port = ntohs(udph->src_port);
    uint16_t dst_port = ntohs(udph->dst_port);
    uint16_t udp_len = ntohs(udph->len);

    // Проверка длины
    if (udp_len < sizeof(struct udp_header) || udp_len > payload_len) {
        return;
    }

    // Проверка чек-суммы (если не ноль)
    if (udph->checksum != 0) {
        uint16_t calc = udp_checksum(
            payload, udp_len,
            iph->src_ip, iph->dst_ip,
            udp_len
        );
        if (calc != 0) {
            printf("[UDP] Checksum mismatch! Expected %x, got %x\n",
                   udph->checksum, calc);
            return;
        }
    }

    // Извлекаем данные
    size_t data_len = udp_len - sizeof(struct udp_header);
    uint8_t* data = payload + sizeof(struct udp_header);

    // Найти сокет по порту назначения
    struct udp_socket* sock = udp_get_socket_by_port(dst_port);
    if (!sock) {
        // Порт не слушается — игнорируем (или отправить ICMP Port Unreachable)
        return;
    }

    // Создаём структуру пакета
    size_t pkt_size = sizeof(struct udp_packet) + data_len;
    struct udp_packet* pkt = (struct udp_packet*)kmalloc(pkt_size);
    if (!pkt) return;

    pkt->src_ip = ntohl(iph->src_ip);
    pkt->src_port = src_port;
    pkt->dst_port = dst_port;
    pkt->data_len = data_len;
    memcpy(pkt->data, data, data_len);

    // Добавить в очередь сокета
    spin_lock(&sock->lock);
    list_add_tail(&pkt->node, &sock->packet_queue);
    sock->recv_count += data_len;

    // Пробуждаем ожидающие процессы
    wake_up(&sock->recv_wait);
    spin_unlock(&sock->lock);

    // Логирование (опционально)
    // printf("[UDP] Received %zu bytes from %d.%d.%d.%d:%d to port %d\n",
    //        data_len,
    //        (pkt->src_ip >> 24) & 0xFF, (pkt->src_ip >> 16) & 0xFF,
    //        (pkt->src_ip >> 8) & 0xFF, pkt->src_ip & 0xFF,
    //        src_port, dst_port);
}
