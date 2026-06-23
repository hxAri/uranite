
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

struct UraniteThread {
    pthread_t handle;
    int64_t id;
};

struct UraniteMutex {
    pthread_mutex_t handle;
};

static _Atomic int64_t nextThreadId = 1;

UraniteThread* uraniteThreadCreate(void (*entry)(void*), void* arg) {
    UraniteThread* thread = (UraniteThread*)malloc(sizeof(UraniteThread));
    if (!thread) return NULL;
    thread->id = atomic_fetch_add(&nextThreadId, 1);
    int result = pthread_create(&thread->handle, NULL, (void*(*)(void*))entry, arg);
    if (result != 0) {
        free(thread);
        return NULL;
    }
    return thread;
}

void uraniteThreadJoin(UraniteThread* thread) {
    if (thread) {
        pthread_join(thread->handle, NULL);
    }
}

void uraniteThreadDetach(UraniteThread* thread) {
    if (thread) {
        pthread_detach(thread->handle);
    }
}

void uraniteThreadDestroy(UraniteThread* thread) {
    free(thread);
}

int64_t uraniteThreadId(UraniteThread* thread) {
    if (!thread) return -1;
    return thread->id;
}

UraniteMutex* uraniteMutexCreate(void) {
    UraniteMutex* mutex = (UraniteMutex*)malloc(sizeof(UraniteMutex));
    if (!mutex) return NULL;
    pthread_mutex_init(&mutex->handle, NULL);
    return mutex;
}

void uraniteMutexLock(UraniteMutex* mutex) {
    if (mutex) pthread_mutex_lock(&mutex->handle);
}

void uraniteMutexUnlock(UraniteMutex* mutex) {
    if (mutex) pthread_mutex_unlock(&mutex->handle);
}

int uraniteMutexTryLock(UraniteMutex* mutex) {
    if (!mutex) return 0;
    return pthread_mutex_trylock(&mutex->handle) == 0 ? 1 : 0;
}

void uraniteMutexDestroy(UraniteMutex* mutex) {
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

struct UraniteCondVar {
    pthread_cond_t handle;
};

UraniteCondVar* uraniteCondVarCreate(void) {
    UraniteCondVar* cv = (UraniteCondVar*)malloc(sizeof(UraniteCondVar));
    if (!cv) return NULL;
    pthread_cond_init(&cv->handle, NULL);
    return cv;
}

void uraniteCondVarWait(UraniteCondVar* cv, UraniteMutex* mutex) {
    if (cv && mutex) pthread_cond_wait(&cv->handle, &mutex->handle);
}

int uraniteCondVarTimedWait(UraniteCondVar* cv, UraniteMutex* mutex, int64_t timeoutMs) {
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

void uraniteCondVarSignal(UraniteCondVar* cv) {
    if (cv) pthread_cond_signal(&cv->handle);
}

void uraniteCondVarBroadcast(UraniteCondVar* cv) {
    if (cv) pthread_cond_broadcast(&cv->handle);
}

void uraniteCondVarDestroy(UraniteCondVar* cv) {
    if (cv) {
        pthread_cond_destroy(&cv->handle);
        free(cv);
    }
}

// Semaphore (mutex + condvar based for portability)

struct UraniteSemaphore {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int64_t count;
};

UraniteSemaphore* uraniteSemaphoreCreate(int64_t initial) {
    UraniteSemaphore* sem = (UraniteSemaphore*)malloc(sizeof(UraniteSemaphore));
    if (!sem) return NULL;
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
    sem->count = initial;
    return sem;
}

void uraniteSemaphoreAcquire(UraniteSemaphore* sem) {
    if (!sem) return;
    pthread_mutex_lock(&sem->mutex);
    while (sem->count <= 0) {
        pthread_cond_wait(&sem->cond, &sem->mutex);
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
}

int uraniteSemaphoreTryAcquire(UraniteSemaphore* sem) {
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

void uraniteSemaphoreRelease(UraniteSemaphore* sem) {
    if (!sem) return;
    pthread_mutex_lock(&sem->mutex);
    sem->count++;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
}

void uraniteSemaphoreDestroy(UraniteSemaphore* sem) {
    if (sem) {
        pthread_mutex_destroy(&sem->mutex);
        pthread_cond_destroy(&sem->cond);
        free(sem);
    }
}

// ReadWriteLock

struct UraniteRWLock {
    pthread_rwlock_t handle;
};

UraniteRWLock* uraniteRWLockCreate(void) {
    UraniteRWLock* rwl = (UraniteRWLock*)malloc(sizeof(UraniteRWLock));
    if (!rwl) return NULL;
    pthread_rwlock_init(&rwl->handle, NULL);
    return rwl;
}

void uraniteRWLockReadLock(UraniteRWLock* rwl) {
    if (rwl) pthread_rwlock_rdlock(&rwl->handle);
}

void uraniteRWLockWriteLock(UraniteRWLock* rwl) {
    if (rwl) pthread_rwlock_wrlock(&rwl->handle);
}

void uraniteRWLockUnlock(UraniteRWLock* rwl) {
    if (rwl) pthread_rwlock_unlock(&rwl->handle);
}

void uraniteRWLockDestroy(UraniteRWLock* rwl) {
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
