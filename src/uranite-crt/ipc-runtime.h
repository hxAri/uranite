
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

#ifndef URANITE_IPC_RUNTIME_H
#define URANITE_IPC_RUNTIME_H

#include <stdint.h>

// Pipe — unidirectional byte stream between processes/threads
typedef struct UranitePipe UranitePipe;

UranitePipe* uranitePipeCreate(void);
int64_t uranitePipeRead(UranitePipe* pipe, char* buffer, int64_t maxBytes);
int64_t uranitePipeWrite(UranitePipe* pipe, const char* data, int64_t length);
void uranitePipeCloseRead(UranitePipe* pipe);
void uranitePipeCloseWrite(UranitePipe* pipe);
void uranitePipeDestroy(UranitePipe* pipe);
int uranitePipeReadFd(UranitePipe* pipe);
int uranitePipeWriteFd(UranitePipe* pipe);

// Shared Memory — named memory region accessible by multiple processes
typedef struct UraniteSharedMemory UraniteSharedMemory;

UraniteSharedMemory* uraniteSharedMemoryCreate(const char* name, int64_t size);
UraniteSharedMemory* uraniteSharedMemoryOpen(const char* name, int64_t size);
void* uraniteSharedMemoryPtr(UraniteSharedMemory* shm);
int64_t uraniteSharedMemorySize(UraniteSharedMemory* shm);
void uraniteSharedMemoryClose(UraniteSharedMemory* shm);
void uraniteSharedMemoryUnlink(const char* name);

#endif
