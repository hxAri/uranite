
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

#ifndef URANITE_SUBPROCESS_RUNTIME_H
#define URANITE_SUBPROCESS_RUNTIME_H

#include <stdint.h>

typedef struct AetherProcess AetherProcess;

typedef struct {
    int64_t exitCode;
    char* standardOutput;
    char* standardError;
} AetherProcessResult;

// Execute command and wait for completion, capturing output
AetherProcessResult* uraniteProcessRun(const char* command);

// Execute command without capturing output
int64_t uraniteProcessExec(const char* command);

// Spawn a background process
AetherProcess* uraniteProcessSpawn(const char* command);

// Wait for spawned process to finish
int64_t uraniteProcessWait(AetherProcess* process);

// Kill a spawned process
void uraniteProcessKill(AetherProcess* process);

// Check if process is still running
int uraniteProcessIsRunning(AetherProcess* process);

// Get PID of spawned process
int64_t uraniteProcessPid(AetherProcess* process);

// Free result struct
void uraniteProcessResultFree(AetherProcessResult* result);

// Free process struct
void uraniteProcessFree(AetherProcess* process);

#endif
