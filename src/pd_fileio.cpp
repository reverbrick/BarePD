//
// pd_fileio.cpp
//
// BarePD - File I/O bridge between libpd (C) and Circle filesystem (C++)
// Copyright (C) 2024 Daniel Górny <PlayableElectronics>
//
// Licensed under GPLv3
//

#include "pd_fileio.h"
#include <circle/fs/fat/fatfs.h>
#include <circle/logger.h>
#include <circle/util.h>

// Avoid including sys/stat.h to prevent time_t conflict with Circle
// Define just what we need
#ifndef S_IFREG
#define S_IFREG 0100000
#endif

static const char FromFileIO[] = "fileio";

// Simple file handle table
#define MAX_OPEN_FILES 8
#define FD_OFFSET 10  // Start file descriptors at 10 to avoid conflicts with stdin/stdout/stderr

struct FileEntry {
    unsigned hFile;      // Circle file handle (0 = unused)
    unsigned nBytesRead; // Total bytes read (for tracking position)
    bool bEOF;           // End of file reached
    bool bValid;
};

static CFATFileSystem *s_pFileSystem = nullptr;
static FileEntry s_FileTable[MAX_OPEN_FILES];

extern "C" {

void pd_fileio_init(void *pFileSystem)
{
    s_pFileSystem = static_cast<CFATFileSystem*>(pFileSystem);
    
    // Clear file table
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        s_FileTable[i].hFile = 0;
        s_FileTable[i].bValid = false;
    }
    
    if (s_pFileSystem) {
        CLogger::Get()->Write(FromFileIO, LogNotice, "File I/O bridge initialized");
    }
}

int pd_fileio_open(const char *path, int flags)
{
    (void)flags;  // We only support read-only for now
    
    if (!s_pFileSystem || !path) {
        return -1;
    }
    
    // Find a free slot
    int slot = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!s_FileTable[i].bValid) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        CLogger::Get()->Write(FromFileIO, LogError, "Too many open files");
        return -1;
    }
    
    // Skip leading slash or dot-slash if present
    const char *pName = path;
    if (pName[0] == '/') pName++;
    if (pName[0] == '.' && pName[1] == '/') pName += 2;
    
    // Open the file
    unsigned hFile = s_pFileSystem->FileOpen(pName);
    if (hFile == 0) {
        CLogger::Get()->Write(FromFileIO, LogWarning, "Cannot open: %s", pName);
        return -1;
    }
    
    // Store in table
    s_FileTable[slot].hFile = hFile;
    s_FileTable[slot].nBytesRead = 0;
    s_FileTable[slot].bEOF = false;
    s_FileTable[slot].bValid = true;
    
    CLogger::Get()->Write(FromFileIO, LogNotice, "Opened: %s (fd=%d)", pName, slot + FD_OFFSET);
    
    return slot + FD_OFFSET;
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
    
    CLogger::Get()->Write(FromFileIO, LogDebug, "Closed fd=%d (read %u bytes)", 
                          fd, s_FileTable[slot].nBytesRead);
    
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
    
    FileEntry *pEntry = &s_FileTable[slot];
    
    // Check if we've already hit EOF
    if (pEntry->bEOF) {
        return 0;
    }
    
    // Read data
    unsigned nRead = s_pFileSystem->FileRead(pEntry->hFile, buf, count);
    
    if (nRead == 0 || nRead == 0xFFFFFFFF) {
        pEntry->bEOF = true;
        return 0;  // EOF
    }
    
    pEntry->nBytesRead += nRead;
    
    return (int)nRead;
}

int pd_fileio_lseek(int fd, int offset, int whence)
{
    int slot = fd - FD_OFFSET;
    
    if (slot < 0 || slot >= MAX_OPEN_FILES || !s_FileTable[slot].bValid) {
        return -1;
    }
    
    // Circle's FAT filesystem doesn't support seeking
    // For SEEK_SET to 0, we could close and reopen, but that's complex
    // Return current position for now (limited functionality)
    
    if (whence == 0 && offset == 0) {
        // SEEK_SET to beginning - would need to reopen file
        CLogger::Get()->Write(FromFileIO, LogWarning, "lseek to start not supported");
    }
    
    return (int)s_FileTable[slot].nBytesRead;
}

int pd_fileio_stat(const char *path, void *statbuf)
{
    if (!s_pFileSystem || !path) {
        return -1;
    }
    
    // Skip leading slash or dot-slash
    const char *pName = path;
    if (pName[0] == '/') pName++;
    if (pName[0] == '.' && pName[1] == '/') pName += 2;
    
    // Try to open to check existence
    unsigned hFile = s_pFileSystem->FileOpen(pName);
    if (hFile == 0) {
        return -1;  // File doesn't exist
    }
    
    s_pFileSystem->FileClose(hFile);
    
    // Fill in minimal stat info - just zero it and set mode
    // We don't use the full struct stat to avoid header conflicts
    if (statbuf) {
        // Zero the buffer (struct stat is typically ~100 bytes)
        memset(statbuf, 0, 128);
        // st_mode is typically at offset 4 or 8 in struct stat
        // Set it to regular file (S_IFREG | permissions)
        // This is a hack but should work for libpd's needs
        unsigned *pMode = (unsigned *)((char *)statbuf + 4);
        *pMode = S_IFREG | 0644;
    }
    
    return 0;
}

}  // extern "C"

