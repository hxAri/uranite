
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

#define _GNU_SOURCE
#define _XOPEN_SOURCE

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <time.h>

#include "uranite-crt/async-runtime.h"

static AetherScheduler globalScheduler;
static int schedulerInitialized = 0;

void uraniteSchedulerInit(void) {
    if (schedulerInitialized) return;
    memset(&globalScheduler, 0, sizeof(globalScheduler));
    globalScheduler.nextTaskId = 1;
    globalScheduler.epollFd = epoll_create1(0);
    if (globalScheduler.epollFd < 0) {
        perror("epoll_create1 failed");
        exit(1);
    }
    schedulerInitialized = 1;
}

static void taskTrampoline(int highBits, int lowBits) {
    AetherTask* task = (AetherTask*)(((uintptr_t)(unsigned int)highBits << 32) | (uintptr_t)(unsigned int)lowBits);
    task->function((void*)task);
    if (task->state != URANITE_TASK_COMPLETED) {
        task->state = URANITE_TASK_COMPLETED;
    }
    swapcontext(&task->context, &globalScheduler.schedulerContext);
}

int64_t uraniteSpawnTask(void (*func)(void*), void* userData) {
    if (!schedulerInitialized) uraniteSchedulerInit();
    if (globalScheduler.taskCount >= URANITE_MAX_TASKS) return -1;

    AetherTask* task = (AetherTask*)calloc(1, sizeof(AetherTask));
    task->id = globalScheduler.nextTaskId++;
    task->state = URANITE_TASK_CREATED;
    task->function = func;
    task->hasResult = 0;
    task->awaitingTaskId = -1;
    task->waitingFd = -1;
    task->userData = userData;

    task->stack = (char*)malloc(URANITE_TASK_STACK_SIZE);
    getcontext(&task->context);
    task->context.uc_stack.ss_sp = task->stack;
    task->context.uc_stack.ss_size = URANITE_TASK_STACK_SIZE;
    task->context.uc_link = &globalScheduler.schedulerContext;

    uintptr_t ptr = (uintptr_t)task;
    int highBits = (int)((ptr >> 32) & 0xFFFFFFFF);
    int lowBits = (int)(ptr & 0xFFFFFFFF);
    makecontext(&task->context, (void(*)(void))taskTrampoline, 2, highBits, lowBits);

    globalScheduler.tasks[globalScheduler.taskCount++] = task;
    return task->id;
}

static AetherTask* findTaskById(int64_t taskId) {
    for (int i = 0; i < globalScheduler.taskCount; i++) {
        if (globalScheduler.tasks[i] && globalScheduler.tasks[i]->id == taskId) {
            return globalScheduler.tasks[i];
        }
    }
    return NULL;
}

void uraniteTaskComplete(void* taskPtr, int64_t result) {
    AetherTask* task = (AetherTask*)taskPtr;
    task->result = result;
    task->hasResult = 1;
    task->state = URANITE_TASK_COMPLETED;
    if (globalScheduler.currentTask == task) {
        swapcontext(&task->context, &globalScheduler.schedulerContext);
    }
}

void uraniteTaskError(void* taskPtr, const char* message) {
    AetherTask* task = (AetherTask*)taskPtr;
    task->hasError = 1;
    task->errorMessage = message;
    task->state = URANITE_TASK_COMPLETED;
    if (globalScheduler.currentTask == task) {
        swapcontext(&task->context, &globalScheduler.schedulerContext);
    }
}

int uraniteTaskHasError(int64_t taskId) {
    AetherTask* target = findTaskById(taskId);
    if (!target) return 0;
    return target->hasError;
}

const char* uraniteTaskGetError(int64_t taskId) {
    AetherTask* target = findTaskById(taskId);
    if (!target) return NULL;
    return target->errorMessage;
}

void uraniteTaskYield(void) {
    AetherTask* task = globalScheduler.currentTask;
    if (!task) return;
    task->state = URANITE_TASK_SUSPENDED;
    swapcontext(&task->context, &globalScheduler.schedulerContext);
}

static int canRunTask(AetherTask* task) {
    if (task->state == URANITE_TASK_COMPLETED || task->state == URANITE_TASK_IO_WAITING) return 0;
    if (task->awaitingTaskId >= 0) {
        AetherTask* awaited = findTaskById(task->awaitingTaskId);
        if (awaited && awaited->state != URANITE_TASK_COMPLETED) return 0;
    }
    return 1;
}

// Run scheduler loop without cleanup — used by uraniteAwaitTask from main context
static void uraniteRunSchedulerLoop(void) {
    struct epoll_event events[URANITE_MAX_EVENTS];

    while (uraniteHasPendingTasks()) {
        int readyTasks = 0;
        for (int i = 0; i < globalScheduler.taskCount; i++) {
            if (globalScheduler.tasks[i] && canRunTask(globalScheduler.tasks[i])) {
                readyTasks++;
            }
        }

        if (readyTasks == 0) {
            int hasIOWaiting = 0;
            for (int i = 0; i < globalScheduler.taskCount; i++) {
                if (globalScheduler.tasks[i] && globalScheduler.tasks[i]->state == URANITE_TASK_IO_WAITING) {
                    hasIOWaiting = 1;
                    break;
                }
            }
            if (!hasIOWaiting) break;

            int nfds = epoll_wait(globalScheduler.epollFd, events, URANITE_MAX_EVENTS, -1);
            for (int n = 0; n < nfds; n++) {
                AetherTask* task = (AetherTask*)events[n].data.ptr;
                task->state = URANITE_TASK_SUSPENDED;
                epoll_ctl(globalScheduler.epollFd, EPOLL_CTL_DEL, task->waitingFd, NULL);
                task->waitingFd = -1;
            }
            continue;
        }

        for (int i = 0; i < globalScheduler.taskCount; i++) {
            AetherTask* task = globalScheduler.tasks[i];
            if (!task || !canRunTask(task)) continue;

            globalScheduler.currentTask = task;
            globalScheduler.currentTaskIndex = i;

            if (task->state == URANITE_TASK_CREATED || task->state == URANITE_TASK_SUSPENDED) {
                task->state = URANITE_TASK_RUNNING;
                swapcontext(&globalScheduler.schedulerContext, &task->context);
            }

            globalScheduler.currentTask = NULL;
        }
    }
}

int64_t uraniteAwaitTask(int64_t taskId) {
    AetherTask* target = findTaskById(taskId);
    if (!target) return 0;
    if (target->state == URANITE_TASK_COMPLETED) return target->result;

    AetherTask* current = globalScheduler.currentTask;
    if (current) {
        current->state = URANITE_TASK_SUSPENDED;
        current->awaitingTaskId = taskId;
        swapcontext(&current->context, &globalScheduler.schedulerContext);
        current->awaitingTaskId = -1;
    } else {
        // Called from main — run loop without cleanup so target stays alive
        uraniteRunSchedulerLoop();
    }
    return target->result;
}

AetherTask* uraniteGetCurrentTask(void) {
    return globalScheduler.currentTask;
}

int uraniteHasPendingTasks(void) {
    for (int i = 0; i < globalScheduler.taskCount; i++) {
        if (globalScheduler.tasks[i] &&
            globalScheduler.tasks[i]->state != URANITE_TASK_COMPLETED) {
            return 1;
        }
    }
    return 0;
}

void uraniteAwaitFD(int fd, uint32_t events) {
    AetherTask* task = globalScheduler.currentTask;
    if (!task) return;

    task->state = URANITE_TASK_IO_WAITING;
    task->waitingFd = fd;
    task->waitingEvents = events;

    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = task;

    if (epoll_ctl(globalScheduler.epollFd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        if (errno == EEXIST) {
            epoll_ctl(globalScheduler.epollFd, EPOLL_CTL_MOD, fd, &ev);
        } else {
            perror("epoll_ctl failed");
            task->state = URANITE_TASK_SUSPENDED;
            return;
        }
    }

    swapcontext(&task->context, &globalScheduler.schedulerContext);
}

void uraniteSleep(uint64_t milliseconds) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tfd < 0) return;

    struct itimerspec ts;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    ts.it_value.tv_sec = milliseconds / 1000;
    ts.it_value.tv_nsec = (milliseconds % 1000) * 1000000;

    if (timerfd_settime(tfd, 0, &ts, NULL) < 0) {
        close(tfd);
        return;
    }

    uraniteAwaitFD(tfd, EPOLLIN);

    epoll_ctl(globalScheduler.epollFd, EPOLL_CTL_DEL, tfd, NULL);
    close(tfd);
}

void uraniteRunScheduler(void) {
    if (!schedulerInitialized) uraniteSchedulerInit();

    uraniteRunSchedulerLoop();

    // Cleanup all tasks
    for (int i = 0; i < globalScheduler.taskCount; i++) {
        if (globalScheduler.tasks[i]) {
            free(globalScheduler.tasks[i]->stack);
            free(globalScheduler.tasks[i]);
            globalScheduler.tasks[i] = NULL;
        }
    }
    globalScheduler.taskCount = 0;
    if (globalScheduler.epollFd >= 0) {
        close(globalScheduler.epollFd);
        globalScheduler.epollFd = -1;
    }
    schedulerInitialized = 0;
}
