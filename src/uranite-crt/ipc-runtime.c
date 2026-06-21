
//
// @author hxAri (hxari)
// @create 2025-02-24 15:15
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ipc-runtime.h"

// Pipe

struct AetherPipe {
    int readFd;
    int writeFd;
};

AetherPipe* uranitePipeCreate(void) {
    AetherPipe* p = (AetherPipe*)malloc(sizeof(AetherPipe));
    if (!p) return NULL;
    int fds[2];
    if (pipe(fds) < 0) {
        free(p);
        return NULL;
    }
    p->readFd = fds[0];
    p->writeFd = fds[1];
    return p;
}

int64_t uranitePipeRead(AetherPipe* p, char* buffer, int64_t maxBytes) {
    if (!p || p->readFd < 0) return -1;
    return (int64_t)read(p->readFd, buffer, (size_t)maxBytes);
}

int64_t uranitePipeWrite(AetherPipe* p, const char* data, int64_t length) {
    if (!p || p->writeFd < 0) return -1;
    return (int64_t)write(p->writeFd, data, (size_t)length);
}

void uranitePipeCloseRead(AetherPipe* p) {
    if (p && p->readFd >= 0) { close(p->readFd); p->readFd = -1; }
}

void uranitePipeCloseWrite(AetherPipe* p) {
    if (p && p->writeFd >= 0) { close(p->writeFd); p->writeFd = -1; }
}

void uranitePipeDestroy(AetherPipe* p) {
    if (p) {
        if (p->readFd >= 0) close(p->readFd);
        if (p->writeFd >= 0) close(p->writeFd);
        free(p);
    }
}

int uranitePipeReadFd(AetherPipe* p) { return p ? p->readFd : -1; }
int uranitePipeWriteFd(AetherPipe* p) { return p ? p->writeFd : -1; }

// Shared Memory

struct AetherSharedMemory {
    char* name;
    void* ptr;
    int64_t size;
    int fd;
};

AetherSharedMemory* uraniteSharedMemoryCreate(const char* name, int64_t size) {
    AetherSharedMemory* shm = (AetherSharedMemory*)calloc(1, sizeof(AetherSharedMemory));
    if (!shm) return NULL;

    shm->name = strdup(name);
    shm->size = size;

    shm->fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (shm->fd < 0) { free(shm->name); free(shm); return NULL; }

    if (ftruncate(shm->fd, size) < 0) {
        close(shm->fd); shm_unlink(name);
        free(shm->name); free(shm);
        return NULL;
    }

    shm->ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->ptr == MAP_FAILED) {
        close(shm->fd); shm_unlink(name);
        free(shm->name); free(shm);
        return NULL;
    }

    return shm;
}

AetherSharedMemory* uraniteSharedMemoryOpen(const char* name, int64_t size) {
    AetherSharedMemory* shm = (AetherSharedMemory*)calloc(1, sizeof(AetherSharedMemory));
    if (!shm) return NULL;

    shm->name = strdup(name);
    shm->size = size;

    shm->fd = shm_open(name, O_RDWR, 0666);
    if (shm->fd < 0) { free(shm->name); free(shm); return NULL; }

    shm->ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->ptr == MAP_FAILED) {
        close(shm->fd);
        free(shm->name); free(shm);
        return NULL;
    }

    return shm;
}

void* uraniteSharedMemoryPtr(AetherSharedMemory* shm) {
    return shm ? shm->ptr : NULL;
}

int64_t uraniteSharedMemorySize(AetherSharedMemory* shm) {
    return shm ? shm->size : 0;
}

void uraniteSharedMemoryClose(AetherSharedMemory* shm) {
    if (shm) {
        if (shm->ptr && shm->ptr != MAP_FAILED) munmap(shm->ptr, shm->size);
        if (shm->fd >= 0) close(shm->fd);
        free(shm->name);
        free(shm);
    }
}

void uraniteSharedMemoryUnlink(const char* name) {
    shm_unlink(name);
}
