
/*
 * @author hxAri (hxari)
 * @create 2026-06-15 03:00
 * @github https://github.com/uranite-lang/uranite
 *
 * Uranite -  *
 * Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
 * Uranite Licence under GNU General Public Licence v3
 *
 * Thin wrappers around POSIX dlfcn.h functions for Uranite's
 * dynamic library loading FFI. Links with -ldl.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <dlfcn.h>
#include <stdint.h>

int64_t uranite_dlopen(const char* libraryPath, int64_t flags) {
    void* handle = dlopen(libraryPath, (int)flags);
    return (int64_t)handle;
}

int64_t uranite_dlsym(int64_t libraryHandle, const char* symbolName) {
    void* symbol = dlsym((void*)libraryHandle, symbolName);
    return (int64_t)symbol;
}

int64_t uranite_dlclose(int64_t libraryHandle) {
    return (int64_t)dlclose((void*)libraryHandle);
}

const char* uranite_dlerror(void) {
    return dlerror();
}
