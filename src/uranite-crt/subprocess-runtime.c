
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

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "uranite-crt/subprocess-runtime.h"

struct AetherProcess {
    pid_t pid;
    int finished;
    int exitCode;
};

AetherProcessResult* uraniteProcessRun(const char* command) {
    AetherProcessResult* result = (AetherProcessResult*)calloc(1, sizeof(AetherProcessResult));
    if (!result) return NULL;

    int stdoutPipe[2], stderrPipe[2];
    if (pipe(stdoutPipe) < 0 || pipe(stderrPipe) < 0) {
        result->exitCode = -1;
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        result->exitCode = -1;
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        close(stderrPipe[0]); close(stderrPipe[1]);
        return result;
    }

    if (pid == 0) {
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);
        execl("/bin/sh", "sh", "-c", command, (char*)NULL);
        _exit(127);
    }

    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    size_t outCap = 4096, outLen = 0;
    size_t errCap = 4096, errLen = 0;
    char* outBuf = (char*)malloc(outCap);
    char* errBuf = (char*)malloc(errCap);

    ssize_t n;
    while ((n = read(stdoutPipe[0], outBuf + outLen, outCap - outLen - 1)) > 0) {
        outLen+= n;
        if (outLen >= outCap - 1) {
            outCap *= 2;
            outBuf = (char*)realloc(outBuf, outCap);
        }
    }
    while ((n = read(stderrPipe[0], errBuf + errLen, errCap - errLen - 1)) > 0) {
        errLen+= n;
        if (errLen >= errCap - 1) {
            errCap *= 2;
            errBuf = (char*)realloc(errBuf, errCap);
        }
    }

    close(stdoutPipe[0]);
    close(stderrPipe[0]);

    outBuf[outLen] = '\0';
    errBuf[errLen] = '\0';

    int status;
    waitpid(pid, &status, 0);
    result->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result->standardOutput = outBuf;
    result->standardError = errBuf;
    return result;
}

int64_t uraniteProcessExec(const char* command) {
    int status = system(command);
    if (status == -1) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

AetherProcess* uraniteProcessSpawn(const char* command) {
    AetherProcess* proc = (AetherProcess*)calloc(1, sizeof(AetherProcess));
    if (!proc) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        free(proc);
        return NULL;
    }
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", command, (char*)NULL);
        _exit(127);
    }

    proc->pid = pid;
    proc->finished = 0;
    proc->exitCode = -1;
    return proc;
}

int64_t uraniteProcessWait(AetherProcess* process) {
    if (!process || process->finished) {
        return process ? process->exitCode : -1;
    }
    int status;
    waitpid(process->pid, &status, 0);
    process->finished = 1;
    process->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return process->exitCode;
}

void uraniteProcessKill(AetherProcess* process) {
    if (process && !process->finished) {
        kill(process->pid, SIGTERM);
    }
}

int uraniteProcessIsRunning(AetherProcess* process) {
    if (!process || process->finished) return 0;
    int status;
    pid_t result = waitpid(process->pid, &status, WNOHANG);
    if (result == 0) return 1;
    process->finished = 1;
    process->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return 0;
}

int64_t uraniteProcessPid(AetherProcess* process) {
    return process ? (int64_t)process->pid : -1;
}

void uraniteProcessResultFree(AetherProcessResult* result) {
    if (result) {
        free(result->standardOutput);
        free(result->standardError);
        free(result);
    }
}

void uraniteProcessFree(AetherProcess* process) {
    free(process);
}
