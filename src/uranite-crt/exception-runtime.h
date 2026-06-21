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
} AetherException;

typedef struct {
    const char* file;
    int64_t line;
    int64_t column;
    const char* function;
} AetherStackFrame;

_Unwind_Reason_Code __uranite_personality_v0(
    int version, _Unwind_Action actions, uint64_t exceptionClass,
    struct _Unwind_Exception* exceptionObject, struct _Unwind_Context* context
);

void __uranite_throw( void* object, const char* typeName ) __attribute__(( noreturn ));
void* __uranite_begin_catch( void* unwind_exception_ptr );
void __uranite_end_catch( void* unwind_exception_ptr );

void __uranite_push_frame( const char* file, int64_t line, int64_t column, const char* function );
void __uranite_pop_frame( void );

#ifdef __cplusplus
}
#endif

#endif
