//
// pd_fileio.cpp
//
// BarePD - File I/O bridge between libpd (C) and Circle filesystem (C++)
// Copyright (C) 2024 Daniel Górny <PlayableElectronics>
//
// This provides the file I/O layer that allows libpd to read patch files
// from the SD card using Circle's FAT filesystem.
//
// Licensed under GPLv3
//

#include "pd_fileio.h"
#include <circle/fs/fat/fatfs.h>
#include <circle/logger.h>
#include <circle/util.h>
#include <stdio.h>

static const char FromFileIO[] = "fileio";

// File handle table
#define MAX_OPEN_FILES 8
#define FD_OFFSET 10  // Start file descriptors at 10 to avoid stdin/stdout/stderr

struct FileEntry {
    unsigned hFile;      // Circle file handle (0 = unused)
    unsigned nPosition;  // Current read position
    unsigned nSize;      // File size (cached on open for SEEK_END support)
    bool bValid;
};

static CFATFileSystem *s_pFileSystem = nullptr;
static FileEntry s_FileTable[MAX_OPEN_FILES];

// Helper: normalize path by removing leading "./" or "/"
static const char *NormalizePath(const char *path)
{
    if (path[0] == '.' && path[1] == '/') return path + 2;
    if (path[0] == '/') return path + 1;
    return path;
}

// Helper: find free slot in file table
static int FindFreeSlot(void)
{
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!s_FileTable[i].bValid) return i;
    }
    return -1;
}

extern "C" {

void pd_fileio_init(void *pFileSystem)
{
    s_pFileSystem = static_cast<CFATFileSystem*>(pFileSystem);
    
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        s_FileTable[i].hFile = 0;
        s_FileTable[i].bValid = false;
    }
    
    CLogger::Get()->Write(FromFileIO, LogDebug, "File I/O initialized");
}

// Called by patched sys_fopen in libpd (not used for patch loading)
FILE *barepd_fopen(const char *filename, const char *mode)
{
    (void)filename; (void)mode;
    return nullptr;  // Patch loading uses barepd_open via sys_open
}

// Called by patched sys_open in libpd - this is the main entry point for patch loading
int barepd_open(const char *path, int oflag)
{
    (void)oflag;  // We only support read-only
    
    if (!s_pFileSystem || !path) {
        return -1;
    }
    
    const char *pName = NormalizePath(path);
    
    int slot = FindFreeSlot();
    if (slot < 0) {
        CLogger::Get()->Write(FromFileIO, LogError, "Too many open files");
        return -1;
    }
    
    // Open file and determine size by reading through it
    unsigned hFile = s_pFileSystem->FileOpen(pName);
    if (hFile == 0) {
        CLogger::Get()->Write(FromFileIO, LogWarning, "Cannot open: %s", pName);
        return -1;
    }
    
    // Get file size (Circle FAT doesn't expose size directly, so read through)
    unsigned nSize = 0;
    char tempBuf[512];
    unsigned nRead;
    while ((nRead = s_pFileSystem->FileRead(hFile, tempBuf, sizeof(tempBuf))) > 0 
           && nRead != FS_ERROR) {
        nSize += nRead;
    }
    s_pFileSystem->FileClose(hFile);
    
    // Reopen for actual reading
    hFile = s_pFileSystem->FileOpen(pName);
    if (hFile == 0) {
        return -1;
    }
    
    s_FileTable[slot].hFile = hFile;
    s_FileTable[slot].nPosition = 0;
    s_FileTable[slot].nSize = nSize;
    s_FileTable[slot].bValid = true;
    
    CLogger::Get()->Write(FromFileIO, LogDebug, "Opened: %s (%u bytes)", pName, nSize);
    return slot + FD_OFFSET;
}

int pd_fileio_open(const char *path, int flags)
{
    return barepd_open(path, flags);
}

int pd_fileio_close(int fd)
{
    int slot = fd - FD_OFFSET;
    
    if (slot < 0 || slot >= MAX_OPEN_FILES || !s_FileTable[slot].bValid) {
        return -1;
    }
    
    if (s_pFileSystem && s_FileTable[slot].hFile) {
        s_pFileSystem->FileClose(s_FileTable[slot].hFile);
    }
    
    s_FileTable[slot].hFile = 0;
    s_FileTable[slot].bValid = false;
    return 0;
}

int pd_fileio_read(int fd, void *buf, unsigned int count)
{
    int slot = fd - FD_OFFSET;
    
    if (slot < 0 || slot >= MAX_OPEN_FILES || !s_FileTable[slot].bValid) {
        return -1;
    }
    
    if (!s_pFileSystem || !buf || count == 0) {
        return -1;
    }
    
    unsigned nRead = s_pFileSystem->FileRead(s_FileTable[slot].hFile, buf, count);
    
    if (nRead == 0 || nRead == FS_ERROR) {
        return 0;  // EOF
    }
    
    s_FileTable[slot].nPosition += nRead;
    return (int)nRead;
}

int pd_fileio_lseek(int fd, int offset, int whence)
{
    int slot = fd - FD_OFFSET;
    
    if (slot < 0 || slot >= MAX_OPEN_FILES || !s_FileTable[slot].bValid) {
        return -1;
    }
    
    FileEntry *pEntry = &s_FileTable[slot];
    
    switch (whence) {
        case 0:  // SEEK_SET
            pEntry->nPosition = (unsigned)offset;
            return offset;
            
        case 1:  // SEEK_CUR
            pEntry->nPosition += offset;
            return (int)pEntry->nPosition;
            
        case 2:  // SEEK_END
            pEntry->nPosition = pEntry->nSize + offset;
            return (int)pEntry->nPosition;
            
        default:
            return -1;
    }
}

int pd_fileio_stat(const char *path, void *statbuf)
{
    if (!s_pFileSystem || !path) {
        return -1;
    }
    
    const char *pName = NormalizePath(path);
    
    // Check existence by trying to open
    unsigned hFile = s_pFileSystem->FileOpen(pName);
    if (hFile == 0) {
        return -1;
    }
    s_pFileSystem->FileClose(hFile);
    
    // Fill minimal stat info
    if (statbuf) {
        memset(statbuf, 0, 128);
        unsigned *pMode = (unsigned *)((char *)statbuf + 4);
        *pMode = 0100000 | 0644;  // S_IFREG | rw-r--r--
    }
    
    return 0;
}

}  // extern "C"
