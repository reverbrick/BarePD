/*
 * arpa/inet.h stub for BarePD bare metal build
 */
#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#include <stdint.h>

typedef uint32_t in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

static inline uint32_t htonl(uint32_t hostlong) {
    return __builtin_bswap32(hostlong);
}

static inline uint16_t htons(uint16_t hostshort) {
    return __builtin_bswap16(hostshort);
}

static inline uint32_t ntohl(uint32_t netlong) {
    return __builtin_bswap32(netlong);
}

static inline uint16_t ntohs(uint16_t netshort) {
    return __builtin_bswap16(netshort);
}

static inline in_addr_t inet_addr(const char *cp) {
    (void)cp;
    return 0;
}

static inline char *inet_ntoa(struct in_addr in) {
    (void)in;
    return "0.0.0.0";
}

#endif /* _ARPA_INET_H */

