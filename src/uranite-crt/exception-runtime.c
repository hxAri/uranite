
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

#include "exception-runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint64_t URANITE_EXCEPTION_CLASS = 0x4145544852000000ULL;

#define URANITE_MAX_STACK_FRAMES 256

static UraniteStackFrame uranite_frame_stack[URANITE_MAX_STACK_FRAMES];
static int uranite_frame_depth = 0;

void __uranite_push_frame( const char* file, int64_t line, int64_t column, const char* function ) {
    if( uranite_frame_depth < URANITE_MAX_STACK_FRAMES ) {
        UraniteStackFrame* frame = &uranite_frame_stack[uranite_frame_depth];
        frame->file = file;
        frame->line = line;
        frame->column = column;
        frame->function = function;
        uranite_frame_depth++;
    }
}

void __uranite_pop_frame( void ) {
    if( uranite_frame_depth > 0 ) {
        uranite_frame_depth--;
    }
}

static void uranite_exception_cleanup( _Unwind_Reason_Code reason, struct _Unwind_Exception* exc ) {
    free( exc );
}

typedef struct {
    int64_t code;
    char* file;
    int64_t line;
    char* message;
} UraniteThrowableLayout;

static void print_source_context( const char* filepath, int64_t errorLine ) {
    if( filepath == NULL || filepath[0] == '\0' || errorLine <= 0 ) {
        return;
    }
    FILE* f = fopen( filepath, "r" );
    if( f == NULL ) {
        return;
    }
    int64_t startLine = errorLine - 1;
    if( startLine < 1 ) {
        startLine = 1;
    }
    int64_t endLine = errorLine + 1;
    char lineBuffer[1024];
    int64_t currentLine = 1;
    while( fgets( lineBuffer, sizeof( lineBuffer ), f ) != NULL ) {
        if( currentLine >= startLine && currentLine <= endLine ) {
            size_t len = strlen( lineBuffer );
            if( len > 0 && lineBuffer[len - 1] == '\n' ) {
                lineBuffer[len - 1] = '\0';
            }
            if( currentLine == errorLine ) {
                fprintf( stderr, "  \033[1;31m> %4ld |\033[0m \033[1;37m%s\033[0m\n", (long)currentLine, lineBuffer );
            }
            else {
                fprintf( stderr, "    %4ld | %s\n", (long)currentLine, lineBuffer );
            }
        }
        if( currentLine > endLine ) {
            break;
        }
        currentLine++;
    }
    fclose( f );
}

static void print_call_stack( void ) {
    if( uranite_frame_depth <= 0 ) {
        return;
    }
    fprintf( stderr, "\033[1;33mCall stack\033[0m (most recent call last):\n" );
    for( int i = 0; i < uranite_frame_depth; i++ ) {
        UraniteStackFrame* frame = &uranite_frame_stack[i];
        const char* funcName = frame->function ? frame->function : "<unknown>";
        if( frame->file != NULL && frame->file[0] != '\0' && frame->line > 0 ) {
            if( frame->column > 0 ) {
                fprintf( stderr, "  \033[1;37m%s\033[0m at \033[1;36m%s:%ld:%ld\033[0m\n",
                    funcName, frame->file, (long)frame->line, (long)frame->column );
            }
            else {
                fprintf( stderr, "  \033[1;37m%s\033[0m at \033[1;36m%s:%ld\033[0m\n",
                    funcName, frame->file, (long)frame->line );
            }
        }
        else {
            fprintf( stderr, "  \033[1;37m%s\033[0m\n", funcName );
        }
    }
}

static void report_unhandled_exception( void* object, const char* typeName ) {
    UraniteThrowableLayout* throwable = (UraniteThrowableLayout*)object;
    const char* name = ( typeName != NULL && typeName[0] != '\0' ) ? typeName : "Exception";
    const char* message = ( throwable->message != NULL ) ? throwable->message : "(no message)";
    const char* file = throwable->file;
    int64_t line = throwable->line;

    fprintf( stderr, "\n" );
    print_call_stack();

    fprintf( stderr, "\n\033[1;31mUnhandled %s\033[0m: %s\n", name, message );
    if( file != NULL && file[0] != '\0' && line > 0 ) {
        fprintf( stderr, "  at \033[1;36m%s:%ld\033[0m\n\n", file, (long)line );
        print_source_context( file, line );
        fprintf( stderr, "\n" );
    }
    else {
        fprintf( stderr, "\n" );
    }
}

void __uranite_throw( void* object, const char* typeName ) {
    UraniteException* exc = (UraniteException*)malloc( sizeof( UraniteException ) );
    memset( &exc->header, 0, sizeof( struct _Unwind_Exception ) );
    exc->header.exception_class = URANITE_EXCEPTION_CLASS;
    exc->header.exception_cleanup = uranite_exception_cleanup;
    exc->uraniteObject = object;
    _Unwind_RaiseException( &exc->header );
    report_unhandled_exception( object, typeName );
    abort();
}

void* __uranite_begin_catch( void* unwind_exception_ptr ) {
    UraniteException* exc = (UraniteException*)unwind_exception_ptr;
    return exc->uraniteObject;
}

void __uranite_end_catch( void* unwind_exception_ptr ) {
    free( unwind_exception_ptr );
}

static uintptr_t read_uleb128( const uint8_t** p ) {
    uintptr_t result = 0;
    int shift = 0;
    uint8_t byte;
    do {
        byte = **p;
        ( *p )++;
        result |= (uintptr_t)( byte & 0x7f ) << shift;
        shift += 7;
    } while( byte & 0x80 );
    return result;
}

static intptr_t read_sleb128( const uint8_t** p ) {
    intptr_t result = 0;
    int shift = 0;
    uint8_t byte;
    do {
        byte = **p;
        ( *p )++;
        result |= (intptr_t)( byte & 0x7f ) << shift;
        shift += 7;
    } while( byte & 0x80 );
    if( shift < (int)( sizeof( intptr_t ) * 8 ) && ( byte & 0x40 ) ) {
        result |= -(((intptr_t)1) << shift);
    }
    return result;
}

_Unwind_Reason_Code __uranite_personality_v0(
    int version, _Unwind_Action actions, uint64_t exceptionClass,
    struct _Unwind_Exception* exceptionObject, struct _Unwind_Context* context
) {
    if( version != 1 ) {
        return _URC_FATAL_PHASE1_ERROR;
    }

    const uint8_t* lsda = (const uint8_t*)_Unwind_GetLanguageSpecificData( context );
    if( !lsda ) {
        return _URC_CONTINUE_UNWIND;
    }

    uintptr_t ip = _Unwind_GetIP( context ) - 1;
    uintptr_t funcStart = _Unwind_GetRegionStart( context );
    uintptr_t ipOffset = ip - funcStart;

    uintptr_t lpStart = funcStart;
    uint8_t lpStartEncoding = *lsda++;
    if( lpStartEncoding != 0xFF ) {
        lpStart = read_uleb128( &lsda );
    }

    uint8_t ttypeEncoding = *lsda++;
    if( ttypeEncoding != 0xFF ) {
        read_uleb128( &lsda );
    }

    uint8_t callSiteEncoding = *lsda++;
    (void)callSiteEncoding;
    uintptr_t callSiteTableLength = read_uleb128( &lsda );
    const uint8_t* callSiteTableEnd = lsda + callSiteTableLength;

    while( lsda < callSiteTableEnd ) {
        uintptr_t csStart = read_uleb128( &lsda );
        uintptr_t csLength = read_uleb128( &lsda );
        uintptr_t csLandingPad = read_uleb128( &lsda );
        uintptr_t csAction = read_uleb128( &lsda );

        if( ipOffset < csStart ) {
            break;
        }

        if( ipOffset >= csStart && ipOffset < csStart + csLength ) {
            if( csLandingPad == 0 ) {
                return _URC_CONTINUE_UNWIND;
            }

            uintptr_t landingPadAddr = lpStart + csLandingPad;

            if( actions & _UA_SEARCH_PHASE ) {
                if( csAction > 0 ) {
                    return _URC_HANDLER_FOUND;
                }
                return _URC_CONTINUE_UNWIND;
            }

            if( actions & _UA_CLEANUP_PHASE ) {
                uintptr_t selector = 0;
                if( actions & _UA_HANDLER_FRAME ) {
                    selector = ( csAction > 0 ) ? 1 : 0;
                }
                else {
                    if( csAction > 0 ) {
                        return _URC_CONTINUE_UNWIND;
                    }
                    selector = 0;
                }

                _Unwind_SetGR( context, __builtin_eh_return_data_regno( 0 ), (uintptr_t)exceptionObject );
                _Unwind_SetGR( context, __builtin_eh_return_data_regno( 1 ), selector );
                _Unwind_SetIP( context, landingPadAddr );
                return _URC_INSTALL_CONTEXT;
            }
        }
    }

    return _URC_CONTINUE_UNWIND;
}
