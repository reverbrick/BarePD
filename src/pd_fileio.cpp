//
// pd_fileio.cpp
//
// BarePD - File I/O bridge between libpd (C) and Circle filesystem (C++)
// Copyright (C) 2024 Daniel Górny <PlayableElectronics>
//
// This provides the file I/O layer that allows libpd to read patch files,
// abstractions, and samples from the SD card using Circle's FAT filesystem.
//
// Licensed under GPLv3
//

#include "pd_fileio.h"
#include <circle/fs/fat/fatfs.h>
#include <circle/logger.h>
#include <circle/util.h>
#include <circle/string.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Forward declaration
extern "C" int barepd_open(const char *path, int oflag);

static const char FromFileIO[] = "fileio";

// File handle table
#define MAX_OPEN_FILES 16
#define MAX_PATH_LEN 256
#define FD_OFFSET 10  // Start file descriptors at 10 to avoid stdin/stdout/stderr

struct FileEntry {
    unsigned hFile;                // Circle file handle (0 = unused)
    unsigned nSize;                // File size (cached on open)
    unsigned nPosition;            // Current logical position (for tracking seeks)
    char szPath[MAX_PATH_LEN];     // File path (for reopening after seek)
    bool bValid;
};

static CFATFileSystem *s_pFileSystem = nullptr;

// Global accessor for other modules
CFATFileSystem *g_pFileSystem = nullptr;
static FileEntry s_FileTable[MAX_OPEN_FILES];

// Helper: normalize path - strip leading "./" or "/" but keep subfolders
static const char *NormalizePath(const char *path)
{
    if (!path) return path;
    
    // Skip "./" prefix
    if (path[0] == '.' && path[1] == '/') path += 2;
    
    // Skip leading "/" (Circle FAT uses relative paths from root)
    while (path[0] == '/') path++;
    
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

// Helper: reopen file and seek to position by reading
static bool ReopenAndSeek(FileEntry *pEntry, unsigned nTargetPos)
{
    // Close current handle
    if (pEntry->hFile) {
        s_pFileSystem->FileClose(pEntry->hFile);
        pEntry->hFile = 0;
    }
    
    // Reopen
    pEntry->hFile = s_pFileSystem->FileOpen(pEntry->szPath);
    if (pEntry->hFile == 0) {
        CLogger::Get()->Write(FromFileIO, LogError, "Failed to reopen: %s", pEntry->szPath);
        return false;
    }
    
    // Skip to target position by reading
    if (nTargetPos > 0) {
        char skipBuf[512];
        unsigned nToSkip = nTargetPos;
        while (nToSkip > 0) {
            unsigned nChunk = (nToSkip > sizeof(skipBuf)) ? sizeof(skipBuf) : nToSkip;
            unsigned nRead = s_pFileSystem->FileRead(pEntry->hFile, skipBuf, nChunk);
            if (nRead == 0 || nRead == FS_ERROR) {
                break;  // EOF or error
            }
            nToSkip -= nRead;
        }
    }
    
    pEntry->nPosition = nTargetPos;
    return true;
}

extern "C" {

void pd_fileio_init(void *pFileSystem)
{
    s_pFileSystem = static_cast<CFATFileSystem*>(pFileSystem);
    g_pFileSystem = s_pFileSystem;  // Set global accessor
    
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        s_FileTable[i].hFile = 0;
        s_FileTable[i].bValid = false;
    }
    
    CLogger::Get()->Write(FromFileIO, LogDebug, "File I/O initialized");
}

// Called by patched sys_fopen in libpd
FILE *barepd_fopen(const char *filename, const char *mode)
{
    // Only support read modes
    if (!mode || (mode[0] != 'r' && mode[0] != 'R')) {
        return nullptr;
    }
    
    int fd = barepd_open(filename, 0);
    if (fd < 0) {
        return nullptr;
    }
    
    // Return fd disguised as FILE* (libpd doesn't use FILE* internals for patch loading)
    return (FILE*)(uintptr_t)fd;
}

// Main file open - used for patches, abstractions, and samples
int barepd_open(const char *path, int oflag)
{
    (void)oflag;  // We only support read-only
    
    if (!s_pFileSystem || !path) {
        return -1;
    }
    
    const char *pName = NormalizePath(path);
    if (!pName || !pName[0]) {
        return -1;
    }
    
    int slot = FindFreeSlot();
    if (slot < 0) {
        CLogger::Get()->Write(FromFileIO, LogError, "Too many open files");
        return -1;
    }
    
    FileEntry *pEntry = &s_FileTable[slot];
    
    // Store path for potential reopening
    strncpy(pEntry->szPath, pName, MAX_PATH_LEN - 1);
    pEntry->szPath[MAX_PATH_LEN - 1] = '\0';
    
    // Open file
    unsigned hFile = s_pFileSystem->FileOpen(pName);
    if (hFile == 0) {
        CLogger::Get()->Write(FromFileIO, LogWarning, "Cannot open: %s", pName);
        return -1;
    }
    
    // Get file size by reading through (Circle FAT doesn't expose size directly)
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
    
    pEntry->hFile = hFile;
    pEntry->nSize = nSize;
    pEntry->nPosition = 0;
    pEntry->bValid = true;
    
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
    
    FileEntry *pEntry = &s_FileTable[slot];
    
    if (s_pFileSystem && pEntry->hFile) {
        s_pFileSystem->FileClose(pEntry->hFile);
    }
    
    pEntry->hFile = 0;
    pEntry->bValid = false;
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
    
    FileEntry *pEntry = &s_FileTable[slot];
    
    unsigned nRead = s_pFileSystem->FileRead(pEntry->hFile, buf, count);
    
    if (nRead == 0 || nRead == FS_ERROR) {
        return 0;  // EOF
    }
    
    pEntry->nPosition += nRead;
    return (int)nRead;
}

int pd_fileio_lseek(int fd, int offset, int whence)
{
    int slot = fd - FD_OFFSET;
    
    if (slot < 0 || slot >= MAX_OPEN_FILES || !s_FileTable[slot].bValid) {
        return -1;
    }
    
    FileEntry *pEntry = &s_FileTable[slot];
    unsigned nNewPos;
    
    switch (whence) {
        case 0:  // SEEK_SET
            nNewPos = (unsigned)offset;
            break;
            
        case 1:  // SEEK_CUR
            nNewPos = pEntry->nPosition + offset;
            break;
            
        case 2:  // SEEK_END
            nNewPos = pEntry->nSize + offset;
            break;
            
        default:
            return -1;
    }
    
    // Clamp to file size
    if (nNewPos > pEntry->nSize) {
        nNewPos = pEntry->nSize;
    }
    
    // If seeking backward, we need to reopen the file
    if (nNewPos < pEntry->nPosition) {
        if (!ReopenAndSeek(pEntry, nNewPos)) {
            return -1;
        }
    } 
    // If seeking forward, just read and discard
    else if (nNewPos > pEntry->nPosition) {
        unsigned nToSkip = nNewPos - pEntry->nPosition;
        char skipBuf[512];
        while (nToSkip > 0) {
            unsigned nChunk = (nToSkip > sizeof(skipBuf)) ? sizeof(skipBuf) : nToSkip;
            unsigned nRead = s_pFileSystem->FileRead(pEntry->hFile, skipBuf, nChunk);
            if (nRead == 0 || nRead == FS_ERROR) {
                break;
            }
            nToSkip -= nRead;
            pEntry->nPosition += nRead;
        }
    }
    // else: nNewPos == pEntry->nPosition, nothing to do
    
    pEntry->nPosition = nNewPos;
    return (int)nNewPos;
}

int pd_fileio_stat(const char *path, void *statbuf)
{
    if (!s_pFileSystem || !path) {
        return -1;
    }
    
    const char *pName = NormalizePath(path);
    
    // Try to open to check existence
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
