//
// pd_fileio.h
//
// BarePD - File I/O bridge between libpd (C) and Circle filesystem (C++)
//

#ifndef _pd_fileio_h
#define _pd_fileio_h

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the file I/O system with Circle's filesystem
// Must be called before libpd tries to open files
void pd_fileio_init(void *pFileSystem);

// File operations (called from pd_compat.c)
int pd_fileio_open(const char *path, int flags);
int pd_fileio_close(int fd);
int pd_fileio_read(int fd, void *buf, unsigned int count);
int pd_fileio_lseek(int fd, int offset, int whence);
int pd_fileio_stat(const char *path, void *statbuf);

#ifdef __cplusplus
}
#endif

#endif



