/*
 * netdb.h stub for BarePD bare metal build
 */
#ifndef _NETDB_H
#define _NETDB_H

#include <stddef.h>
#include "sys/socket.h"

struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

#define h_addr h_addr_list[0]

struct addrinfo {
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
};

#define AI_PASSIVE     0x0001
#define AI_CANONNAME   0x0002
#define AI_NUMERICHOST 0x0004
#define AI_NUMERICSERV 0x0008
#define AI_ADDRCONFIG  0x0020

#define EAI_NONAME     -2
#define EAI_SERVICE    -8
#define EAI_FAIL       -4
#define EAI_MEMORY     -10

static inline struct hostent *gethostbyname(const char *name) {
    (void)name;
    return NULL;
}

static inline int getaddrinfo(const char *node, const char *service,
                              const struct addrinfo *hints,
                              struct addrinfo **res) {
    (void)node; (void)service; (void)hints; (void)res;
    return EAI_FAIL;
}

static inline void freeaddrinfo(struct addrinfo *res) {
    (void)res;
}

static inline const char *gai_strerror(int errcode) {
    (void)errcode;
    return "Address resolution not supported";
}

#endif /* _NETDB_H */

