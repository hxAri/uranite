
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

#ifndef URANITE_ASYNC_RUNTIME_H
#define URANITE_ASYNC_RUNTIME_H

#include <stdint.h>
#include <ucontext.h>

#define URANITE_MAX_TASKS 1024
#define URANITE_TASK_STACK_SIZE (1024 * 64)
#define URANITE_MAX_EVENTS 64

typedef enum {
    URANITE_TASK_CREATED,
    URANITE_TASK_RUNNING,
    URANITE_TASK_SUSPENDED,
    URANITE_TASK_IO_WAITING,
    URANITE_TASK_COMPLETED
} AetherTaskState;

typedef struct AetherTask {
    int64_t id;
    AetherTaskState state;
    void (*function)(void*);
    ucontext_t context;
    char* stack;
    int64_t result;
    int hasResult;
    int hasError;
    const char* errorMessage;
    int64_t awaitingTaskId;
    int waitingFd;
    uint32_t waitingEvents;
    void* userData;
} AetherTask;

typedef struct {
    AetherTask* tasks[URANITE_MAX_TASKS];
    int taskCount;
    int currentTaskIndex;
    AetherTask* currentTask;
    ucontext_t schedulerContext;
    int64_t nextTaskId;
    int epollFd;
} AetherScheduler;

void uraniteSchedulerInit(void);
int64_t uraniteSpawnTask(void (*func)(void*), void* userData);
void uraniteTaskComplete(void* task, int64_t result);
void uraniteTaskYield(void);
int64_t uraniteAwaitTask(int64_t taskId);
void uraniteRunScheduler(void);
int uraniteHasPendingTasks(void);
AetherTask* uraniteGetCurrentTask(void);

// Error propagation
void uraniteTaskError(void* task, const char* message);
int uraniteTaskHasError(int64_t taskId);
const char* uraniteTaskGetError(int64_t taskId);

// Event Loop APIs
void uraniteAwaitFD(int fd, uint32_t events);
void uraniteSleep(uint64_t milliseconds);

#endif
