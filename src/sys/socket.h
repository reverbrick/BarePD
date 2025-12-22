/*
 * sys/socket.h stub for BarePD bare metal build
 */
#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t socklen_t;

#define AF_INET    2
#define AF_INET6   10
#define AF_UNIX    1
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOL_SOCKET  1
#define SO_REUSEADDR 2
#define SO_BROADCAST 6
#define MSG_DONTWAIT 0x40

#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
};

/* sockaddr_storage - large enough for any address family */
struct sockaddr_storage {
    uint16_t ss_family;
    char __ss_padding[126];
};

/* These are just stubs - they return error */
static inline int socket(int domain, int type, int protocol) {
    (void)domain; (void)type; (void)protocol;
    return -1;
}

static inline int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void)sockfd; (void)addr; (void)addrlen;
    return -1;
}

static inline int listen(int sockfd, int backlog) {
    (void)sockfd; (void)backlog;
    return -1;
}

static inline int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    (void)sockfd; (void)addr; (void)addrlen;
    return -1;
}

static inline int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void)sockfd; (void)addr; (void)addrlen;
    return -1;
}

static inline ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    (void)sockfd; (void)buf; (void)len; (void)flags;
    return -1;
}

static inline ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    (void)sockfd; (void)buf; (void)len; (void)flags;
    return -1;
}

static inline ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                             const struct sockaddr *dest_addr, socklen_t addrlen) {
    (void)sockfd; (void)buf; (void)len; (void)flags;
    (void)dest_addr; (void)addrlen;
    return -1;
}

static inline ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                               struct sockaddr *src_addr, socklen_t *addrlen) {
    (void)sockfd; (void)buf; (void)len; (void)flags;
    (void)src_addr; (void)addrlen;
    return -1;
}

static inline int setsockopt(int sockfd, int level, int optname,
                             const void *optval, socklen_t optlen) {
    (void)sockfd; (void)level; (void)optname;
    (void)optval; (void)optlen;
    return 0;
}

static inline int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    (void)sockfd; (void)addr; (void)addrlen;
    return -1;
}

static inline int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    (void)sockfd; (void)addr; (void)addrlen;
    return -1;
}

static inline int shutdown(int sockfd, int how) {
    (void)sockfd; (void)how;
    return 0;
}

#endif /* _SYS_SOCKET_H */

