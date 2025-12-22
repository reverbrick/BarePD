/*
 * sys/mman.h stub for BarePD bare metal build
 */
#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#include <stddef.h>

#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4
#define PROT_NONE  0x0

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20
#define MAP_ANON      MAP_ANONYMOUS
#define MAP_FAILED    ((void *)-1)

static inline void *mmap(void *addr, size_t length, int prot, int flags,
                         int fd, long offset) {
    (void)addr; (void)length; (void)prot; (void)flags;
    (void)fd; (void)offset;
    return MAP_FAILED;
}

static inline int munmap(void *addr, size_t length) {
    (void)addr; (void)length;
    return -1;
}

#endif /* _SYS_MMAN_H */

