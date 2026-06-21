
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

#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 700

#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>

#include "uranite-crt/thread-runtime.h"

struct AetherThread {
    pthread_t handle;
    int64_t id;
};

struct AetherMutex {
    pthread_mutex_t handle;
};

static _Atomic int64_t nextThreadId = 1;

AetherThread* uraniteThreadCreate(void (*entry)(void*), void* arg) {
    AetherThread* thread = (AetherThread*)malloc(sizeof(AetherThread));
    if (!thread) return NULL;
    thread->id = atomic_fetch_add(&nextThreadId, 1);
    int result = pthread_create(&thread->handle, NULL, (void*(*)(void*))entry, arg);
    if (result != 0) {
        free(thread);
        return NULL;
    }
    return thread;
}

void uraniteThreadJoin(AetherThread* thread) {
    if (thread) {
        pthread_join(thread->handle, NULL);
    }
}

void uraniteThreadDetach(AetherThread* thread) {
    if (thread) {
        pthread_detach(thread->handle);
    }
}

void uraniteThreadDestroy(AetherThread* thread) {
    free(thread);
}

int64_t uraniteThreadId(AetherThread* thread) {
    if (!thread) return -1;
    return thread->id;
}

AetherMutex* uraniteMutexCreate(void) {
    AetherMutex* mutex = (AetherMutex*)malloc(sizeof(AetherMutex));
    if (!mutex) return NULL;
    pthread_mutex_init(&mutex->handle, NULL);
    return mutex;
}

void uraniteMutexLock(AetherMutex* mutex) {
    if (mutex) pthread_mutex_lock(&mutex->handle);
}

void uraniteMutexUnlock(AetherMutex* mutex) {
    if (mutex) pthread_mutex_unlock(&mutex->handle);
}

int uraniteMutexTryLock(AetherMutex* mutex) {
    if (!mutex) return 0;
    return pthread_mutex_trylock(&mutex->handle) == 0 ? 1 : 0;
}

void uraniteMutexDestroy(AetherMutex* mutex) {
    if (mutex) {
        pthread_mutex_destroy(&mutex->handle);
        free(mutex);
    }
}

int64_t uraniteAtomicLoad(int64_t* ptr) {
    return atomic_load((_Atomic int64_t*)ptr);
}

void uraniteAtomicStore(int64_t* ptr, int64_t val) {
    atomic_store((_Atomic int64_t*)ptr, val);
}

int64_t uraniteAtomicAdd(int64_t* ptr, int64_t val) {
    return atomic_fetch_add((_Atomic int64_t*)ptr, val);
}

int64_t uraniteAtomicSub(int64_t* ptr, int64_t val) {
    return atomic_fetch_sub((_Atomic int64_t*)ptr, val);
}

int uraniteAtomicCompareExchange(int64_t* ptr, int64_t* expected, int64_t desired) {
    return atomic_compare_exchange_strong((_Atomic int64_t*)ptr, expected, desired) ? 1 : 0;
}

// ConditionVariable

struct AetherCondVar {
    pthread_cond_t handle;
};

AetherCondVar* uraniteCondVarCreate(void) {
    AetherCondVar* cv = (AetherCondVar*)malloc(sizeof(AetherCondVar));
    if (!cv) return NULL;
    pthread_cond_init(&cv->handle, NULL);
    return cv;
}

void uraniteCondVarWait(AetherCondVar* cv, AetherMutex* mutex) {
    if (cv && mutex) pthread_cond_wait(&cv->handle, &mutex->handle);
}

int uraniteCondVarTimedWait(AetherCondVar* cv, AetherMutex* mutex, int64_t timeoutMs) {
    if (!cv || !mutex) return 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec+= timeoutMs / 1000;
    ts.tv_nsec+= (timeoutMs % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec+= 1;
        ts.tv_nsec-= 1000000000;
    }
    return pthread_cond_timedwait(&cv->handle, &mutex->handle, &ts) == 0 ? 1 : 0;
}

void uraniteCondVarSignal(AetherCondVar* cv) {
    if (cv) pthread_cond_signal(&cv->handle);
}

void uraniteCondVarBroadcast(AetherCondVar* cv) {
    if (cv) pthread_cond_broadcast(&cv->handle);
}

void uraniteCondVarDestroy(AetherCondVar* cv) {
    if (cv) {
        pthread_cond_destroy(&cv->handle);
        free(cv);
    }
}

// Semaphore (mutex + condvar based for portability)

struct AetherSemaphore {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int64_t count;
};

AetherSemaphore* uraniteSemaphoreCreate(int64_t initial) {
    AetherSemaphore* sem = (AetherSemaphore*)malloc(sizeof(AetherSemaphore));
    if (!sem) return NULL;
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
    sem->count = initial;
    return sem;
}

void uraniteSemaphoreAcquire(AetherSemaphore* sem) {
    if (!sem) return;
    pthread_mutex_lock(&sem->mutex);
    while (sem->count <= 0) {
        pthread_cond_wait(&sem->cond, &sem->mutex);
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
}

int uraniteSemaphoreTryAcquire(AetherSemaphore* sem) {
    if (!sem) return 0;
    pthread_mutex_lock(&sem->mutex);
    if (sem->count > 0) {
        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return 1;
    }
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

void uraniteSemaphoreRelease(AetherSemaphore* sem) {
    if (!sem) return;
    pthread_mutex_lock(&sem->mutex);
    sem->count++;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
}

void uraniteSemaphoreDestroy(AetherSemaphore* sem) {
    if (sem) {
        pthread_mutex_destroy(&sem->mutex);
        pthread_cond_destroy(&sem->cond);
        free(sem);
    }
}

// ReadWriteLock

struct AetherRWLock {
    pthread_rwlock_t handle;
};

AetherRWLock* uraniteRWLockCreate(void) {
    AetherRWLock* rwl = (AetherRWLock*)malloc(sizeof(AetherRWLock));
    if (!rwl) return NULL;
    pthread_rwlock_init(&rwl->handle, NULL);
    return rwl;
}

void uraniteRWLockReadLock(AetherRWLock* rwl) {
    if (rwl) pthread_rwlock_rdlock(&rwl->handle);
}

void uraniteRWLockWriteLock(AetherRWLock* rwl) {
    if (rwl) pthread_rwlock_wrlock(&rwl->handle);
}

void uraniteRWLockUnlock(AetherRWLock* rwl) {
    if (rwl) pthread_rwlock_unlock(&rwl->handle);
}

void uraniteRWLockDestroy(AetherRWLock* rwl) {
    if (rwl) {
        pthread_rwlock_destroy(&rwl->handle);
        free(rwl);
    }
}

// Sleep

void uraniteThreadSleep(int64_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}
