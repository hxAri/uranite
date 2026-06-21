
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

#ifndef URANITE_THREAD_RUNTIME_H
#define URANITE_THREAD_RUNTIME_H

#include <stdint.h>

typedef struct AetherThread AetherThread;
typedef struct AetherMutex AetherMutex;

// Thread creation and management
AetherThread* uraniteThreadCreate(void (*entry)(void*), void* arg);
void uraniteThreadJoin(AetherThread* thread);
void uraniteThreadDetach(AetherThread* thread);
void uraniteThreadDestroy(AetherThread* thread);
int64_t uraniteThreadId(AetherThread* thread);

// Mutex
AetherMutex* uraniteMutexCreate(void);
void uraniteMutexLock(AetherMutex* mutex);
void uraniteMutexUnlock(AetherMutex* mutex);
int uraniteMutexTryLock(AetherMutex* mutex);
void uraniteMutexDestroy(AetherMutex* mutex);

// Atomics
int64_t uraniteAtomicLoad(int64_t* ptr);
void uraniteAtomicStore(int64_t* ptr, int64_t val);
int64_t uraniteAtomicAdd(int64_t* ptr, int64_t val);
int64_t uraniteAtomicSub(int64_t* ptr, int64_t val);
int uraniteAtomicCompareExchange(int64_t* ptr, int64_t* expected, int64_t desired);

// ConditionVariable
typedef struct AetherCondVar AetherCondVar;
AetherCondVar* uraniteCondVarCreate(void);
void uraniteCondVarWait(AetherCondVar* cv, AetherMutex* mutex);
int uraniteCondVarTimedWait(AetherCondVar* cv, AetherMutex* mutex, int64_t timeoutMs);
void uraniteCondVarSignal(AetherCondVar* cv);
void uraniteCondVarBroadcast(AetherCondVar* cv);
void uraniteCondVarDestroy(AetherCondVar* cv);

// Semaphore
typedef struct AetherSemaphore AetherSemaphore;
AetherSemaphore* uraniteSemaphoreCreate(int64_t initial);
void uraniteSemaphoreAcquire(AetherSemaphore* sem);
int uraniteSemaphoreTryAcquire(AetherSemaphore* sem);
void uraniteSemaphoreRelease(AetherSemaphore* sem);
void uraniteSemaphoreDestroy(AetherSemaphore* sem);

// ReadWriteLock
typedef struct AetherRWLock AetherRWLock;
AetherRWLock* uraniteRWLockCreate(void);
void uraniteRWLockReadLock(AetherRWLock* rwl);
void uraniteRWLockWriteLock(AetherRWLock* rwl);
void uraniteRWLockUnlock(AetherRWLock* rwl);
void uraniteRWLockDestroy(AetherRWLock* rwl);

// Sleep
void uraniteThreadSleep(int64_t milliseconds);

#endif
