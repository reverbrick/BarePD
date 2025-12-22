/*
 * netinet/in.h stub for BarePD bare metal build
 */
#ifndef _NETINET_IN_H
#define _NETINET_IN_H

#include <stdint.h>
#include "../arpa/inet.h"

#define INADDR_ANY       ((in_addr_t)0x00000000)
#define INADDR_BROADCAST ((in_addr_t)0xffffffff)
#define INADDR_NONE      ((in_addr_t)0xffffffff)

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

#define AF_INET 2
#define PF_INET AF_INET

#endif /* _NETINET_IN_H */

