
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

#ifndef URANITE_EXCEPTION_RUNTIME_H
#define URANITE_EXCEPTION_RUNTIME_H

#include <unwind.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    struct _Unwind_Exception header;
    void* uraniteObject;
} UraniteException;

typedef struct {
    const char* file;
    int64_t line;
    int64_t column;
    const char* function;
} UraniteStackFrame;

_Unwind_Reason_Code __uranite_personality_v0(
    int version, _Unwind_Action actions, uint64_t exceptionClass,
    struct _Unwind_Exception* exceptionObject, struct _Unwind_Context* context
);

void __uranite_throw( void* object, const char* typeName ) __attribute__(( noreturn ));
void* __uranite_begin_catch( void* unwind_exception_ptr );
void __uranite_end_catch( void* unwind_exception_ptr );

void __uranite_push_frame( const char* file, int64_t line, int64_t column, const char* function );
void __uranite_pop_frame( void );

int64_t __uranite_get_frame_depth( void );
UraniteStackFrame* __uranite_get_frame_at( int64_t index );

#ifdef __cplusplus
}
#endif

#endif
