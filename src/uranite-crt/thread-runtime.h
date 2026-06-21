
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

typedef struct UraniteThread UraniteThread;
typedef struct UraniteMutex UraniteMutex;

// Thread creation and management
UraniteThread* uraniteThreadCreate(void (*entry)(void*), void* arg);
void uraniteThreadJoin(UraniteThread* thread);
void uraniteThreadDetach(UraniteThread* thread);
void uraniteThreadDestroy(UraniteThread* thread);
int64_t uraniteThreadId(UraniteThread* thread);

// Mutex
UraniteMutex* uraniteMutexCreate(void);
void uraniteMutexLock(UraniteMutex* mutex);
void uraniteMutexUnlock(UraniteMutex* mutex);
int uraniteMutexTryLock(UraniteMutex* mutex);
void uraniteMutexDestroy(UraniteMutex* mutex);

// Atomics
int64_t uraniteAtomicLoad(int64_t* ptr);
void uraniteAtomicStore(int64_t* ptr, int64_t val);
int64_t uraniteAtomicAdd(int64_t* ptr, int64_t val);
int64_t uraniteAtomicSub(int64_t* ptr, int64_t val);
int uraniteAtomicCompareExchange(int64_t* ptr, int64_t* expected, int64_t desired);

// ConditionVariable
typedef struct UraniteCondVar UraniteCondVar;
UraniteCondVar* uraniteCondVarCreate(void);
void uraniteCondVarWait(UraniteCondVar* cv, UraniteMutex* mutex);
int uraniteCondVarTimedWait(UraniteCondVar* cv, UraniteMutex* mutex, int64_t timeoutMs);
void uraniteCondVarSignal(UraniteCondVar* cv);
void uraniteCondVarBroadcast(UraniteCondVar* cv);
void uraniteCondVarDestroy(UraniteCondVar* cv);

// Semaphore
typedef struct UraniteSemaphore UraniteSemaphore;
UraniteSemaphore* uraniteSemaphoreCreate(int64_t initial);
void uraniteSemaphoreAcquire(UraniteSemaphore* sem);
int uraniteSemaphoreTryAcquire(UraniteSemaphore* sem);
void uraniteSemaphoreRelease(UraniteSemaphore* sem);
void uraniteSemaphoreDestroy(UraniteSemaphore* sem);

// ReadWriteLock
typedef struct UraniteRWLock UraniteRWLock;
UraniteRWLock* uraniteRWLockCreate(void);
void uraniteRWLockReadLock(UraniteRWLock* rwl);
void uraniteRWLockWriteLock(UraniteRWLock* rwl);
void uraniteRWLockUnlock(UraniteRWLock* rwl);
void uraniteRWLockDestroy(UraniteRWLock* rwl);

// Sleep
void uraniteThreadSleep(int64_t milliseconds);

#endif
