/*
 * pd_compat.c
 *
 * BarePD - Bare metal compatibility layer for libpd
 * Copyright (C) 2024 Daniel Górny <PlayableElectronics>
 * 
 * This file provides stub implementations for functions that newlib
 * needs but aren't available in the bare-metal Circle environment.
 *
 * Licensed under GPLv3
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

/* File I/O bridge - implemented in pd_fileio.cpp */
#include "pd_fileio.h"

/* Newlib system call stubs - these are called by newlib's libc */

/* _sbrk - increase program data space for malloc
 * Note: Circle also provides malloc in libcircle.a
 * This is used by newlib's malloc if it gets linked
 */
extern char _end;  /* Defined by linker - end of BSS */
static char *heap_end = 0;

void *_sbrk(ptrdiff_t incr) {
    char *prev_heap_end;
    
    if (heap_end == 0) {
        heap_end = &_end;
    }
    prev_heap_end = heap_end;
    heap_end += incr;
    
    return (void *)prev_heap_end;
}

/* _write - write to a file descriptor */
int _write(int fd, const void *buf, size_t count) {
    (void)fd;
    (void)buf;
    /* Could redirect stdout/stderr to Circle's logger */
    return count;  /* Pretend we wrote everything */
}

/* _read - read from a file descriptor using Circle's filesystem */
int _read(int fd, void *buf, size_t count) {
    /* fd 0,1,2 are stdin/stdout/stderr */
    if (fd < 3) {
        return 0;  /* EOF for stdin */
    }
    return pd_fileio_read(fd, buf, count);
}

/* _close - close a file descriptor */
int _close(int fd) {
    if (fd < 3) {
        return 0;  /* Can't close stdin/stdout/stderr */
    }
    return pd_fileio_close(fd);
}

/* _lseek - reposition read/write file offset */
off_t _lseek(int fd, off_t offset, int whence) {
    if (fd < 3) {
        return -1;
    }
    return pd_fileio_lseek(fd, offset, whence);
}

/* _fstat - get file status */
int _fstat(int fd, struct stat *st) {
    (void)fd;
    if (st) {
        st->st_mode = S_IFCHR;  /* Character device */
    }
    return 0;
}

/* _isatty - is this a terminal */
int _isatty(int fd) {
    (void)fd;
    return 1;  /* Pretend everything is a terminal */
}

/* _getpid - get process ID */
int _getpid(void) {
    return 1;
}

/* _kill - send signal to a process */
int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    return -1;
}

/* _exit - exit the program */
void _exit(int status) {
    (void)status;
    while(1) { }  /* Hang forever in bare metal */
}

/* signal() is provided by newlib */
#include <signal.h>

/* sigaction stub - newlib declares it but doesn't implement it */
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    (void)signum;
    (void)act;
    (void)oldact;
    return 0;
}

/* Environment stubs */
char *getenv(const char *name) {
    (void)name;
    return NULL;
}

/* Socket stubs - pd doesn't need networking in our use case */
int socket(int domain, int type, int protocol) {
    (void)domain;
    (void)type;
    (void)protocol;
    return -1;
}

int bind(int sockfd, const void *addr, unsigned int addrlen) {
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    return -1;
}

int connect(int sockfd, const void *addr, unsigned int addrlen) {
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    return -1;
}

int listen(int sockfd, int backlog) {
    (void)sockfd;
    (void)backlog;
    return -1;
}

int accept(int sockfd, void *addr, void *addrlen) {
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    return -1;
}

long send(int sockfd, const void *buf, size_t len, int flags) {
    (void)sockfd;
    (void)buf;
    (void)len;
    (void)flags;
    return -1;
}

long recv(int sockfd, void *buf, size_t len, int flags) {
    (void)sockfd;
    (void)buf;
    (void)len;
    (void)flags;
    return -1;
}

/* Dynamic loading stubs - not needed for built-in externals */
void *dlopen(const char *filename, int flags) {
    (void)filename;
    (void)flags;
    return NULL;
}

void *dlsym(void *handle, const char *symbol) {
    (void)handle;
    (void)symbol;
    return NULL;
}

int dlclose(void *handle) {
    (void)handle;
    return 0;
}

char *dlerror(void) {
    return "Dynamic loading not supported";
}

/* Note: sys_lock, sys_unlock, sys_pollgui, sys_init_fdpoll are 
   provided by pd's s_inter.c */

/* Directory operations stubs */
typedef void DIR;
struct dirent {
    char d_name[256];
};

DIR *opendir(const char *name) {
    (void)name;
    return NULL;
}

struct dirent *readdir(DIR *dirp) {
    (void)dirp;
    return NULL;
}

int closedir(DIR *dirp) {
    (void)dirp;
    return 0;
}

/* stat - get file status using Circle's filesystem */
int stat(const char *pathname, struct stat *statbuf) {
    return pd_fileio_stat(pathname, statbuf);
}

/* getcwd stub */
char *getcwd(char *buf, size_t size) {
    if (buf && size > 1) {
        buf[0] = '/';
        buf[1] = '\0';
        return buf;
    }
    return NULL;
}

/* chdir stub */
int chdir(const char *path) {
    (void)path;
    return 0;
}

/* Unix system stubs */
int getuid(void) { return 0; }
int geteuid(void) { return 0; }
int setuid(int uid) { (void)uid; return 0; }
int readlink(const char *path, char *buf, size_t bufsiz) {
    (void)path; (void)buf; (void)bufsiz;
    return -1;
}
int usleep(unsigned int usec) { (void)usec; return 0; }

/* setitimer, select - provide implementations */
#include <sys/select.h>

int setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    (void)which; (void)new_value; (void)old_value;
    return 0;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    (void)nfds; (void)readfds; (void)writefds; (void)exceptfds; (void)timeout;
    return 0;
}

/* Socket helper stubs */
int socket_init(void) { return 0; }
int socket_errno(void) { return 0; }
const char *socket_strerror(int err) { (void)err; return "no error"; }
int socket_close(int fd) { (void)fd; return 0; }
int socket_errno_udp(void) { return 0; }

/* addrinfo stubs */
int addrinfo_get_list(void **ailist, const char *hostname, int port, int flags) {
    (void)ailist; (void)hostname; (void)port; (void)flags;
    return -1;
}
void addrinfo_sort_list(void **ailist, int (*compare)(const void*, const void*)) {
    (void)ailist; (void)compare;
}
int addrinfo_ipv4_first(const void* ai1, const void* ai2) {
    (void)ai1; (void)ai2;
    return 0;
}

/* Pure Data optional components - stub setup functions */
void x_net_setup(void) { }
void x_file_setup(void) { }
void d_soundfile_setup(void) { }

/* _open - open a file using Circle's filesystem */
int _open(const char *path, int flags, ...) {
    return pd_fileio_open(path, flags);
}

/* _fini stub */
void _fini(void) { }

/* gettimeofday stub */
int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (tv) {
        tv->tv_sec = 0;
        tv->tv_usec = 0;
    }
    return 0;
}

/* pthread stubs - single-threaded bare metal environment */
#include <sys/types.h>

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void)mutex; (void)attr;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    (void)mutex;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    (void)mutex;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    (void)mutex;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    (void)mutex;
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    (void)cond; (void)attr;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    (void)cond;
    return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    (void)cond; (void)mutex;
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           const struct timespec *abstime) {
    (void)cond; (void)mutex; (void)abstime;
    return 0;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    (void)cond;
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    (void)cond;
    return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    (void)thread; (void)attr; (void)start_routine; (void)arg;
    return -1;  /* Threads not supported */
}

int pthread_join(pthread_t thread, void **retval) {
    (void)thread; (void)retval;
    return 0;
}

pthread_t pthread_self(void) {
    return 0;
}

int pthread_equal(pthread_t t1, pthread_t t2) {
    return t1 == t2;
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    (void)key; (void)destructor;
    return 0;
}

int pthread_key_delete(pthread_key_t key) {
    (void)key;
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    (void)key;
    return NULL;
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    (void)key; (void)value;
    return 0;
}
