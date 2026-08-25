#!/usr/bin/env bash

# 
# @author hxAri (hxari)
# @create 2026-08-24 00:00
# @update 2026-08-25 02:42
# @github https://github.com/uranite-lang/uranite
# 
# Memory-leak spot-check for Uranite stdlib modules, usable where valgrind
# is unavailable. Builds an LD_PRELOAD malloc/calloc/free interposer, runs
# synthetic workloads against the current stdlib tree (and optionally against
# HEAD's stdlib as a baseline), and reports outstanding-allocation slopes
# (delta allocations per iteration). A fix that releases a per-iteration
# allocation lowers the "before" slope; balanced create/destroy pairs show
# slope zero or vanish entirely once the optimizer proves the pairing.
# 
# Usage: bash scripts/uranite-memcheck.sh [OPTIONS]
# 
# Options:
#   --baseline          Compare against a committed stdlibs snapshot via git archive
#   --baseline-ref REF  Git revision for --baseline (default HEAD)
#   --n1 COUNT          First iteration count (default 2000)
#   --n2 COUNT          Second iteration count (default 8000)
#   --workload NAME     Run only the named workload (repeatable)
#   --list              List available workloads and exit
#   --keep              Keep temporary artifacts instead of deleting them
#   --help              Show this help text
# 
# Workloads:
#   c_sanity            C reference leak proving the detector works
#   w1_readlink_err     io.path readlink error-path buffer release
#   w2_subprocess_run   subprocess run/wait status buffer release
#   w3_router_resolve   web.router match-path segment buffers
#   w4_tuple            collection.tuple data buffer paired destruction
#   w5_scheduler        async.scheduler destroy slot buffers
#   minimal_raise       raise/catch churn regression guard
# 
# Notes:
#   - Requires gcc and the built compiler at ./build/uranite.
#   - --baseline requires git; workloads that call newly added APIs fail to
#     link against the baseline (expected: the API did not exist at HEAD).
#   - The MIR optimizer erases provably-paired or non-escaping allocations,
#     so zero heap traffic means "provably balanced", never "not executed".
# 
# Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
# Uranite Licence under GNU General Public Licence v3
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
# 
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
# 

set -u

# Subshell status code
# Just for container last subshell exit code.
SUBSHELLSTATUS=

# Current filename.
__name__="$0"

# Target basename.
pathname=$(basename $__name__)

# Change current working directory.
cd $(dirname $__name__)

# Iterate down a (possible) chain of symlinks.
while [ -L "$pathname" ]
do
	pathname=$(readlink $pathname)
	cd $(dirname $pathname)
	pathname=$(basename $pathname)
done

# Clear the terminal screens.
clear

# Change current working directory into parent directory.
basepath=$(cd .. && pwd)

# For compatibilty system.
if [[ ! $(command -v puts) ]]; then
	
	# echo -e is a pretty command line output (just for my os only)
	function puts() {
		echo -e "\x1b[0m$@"
	}
fi

scriptPath="$basepath/scripts/$pathname"

compilerBinary="$basepath/build/uranite"
memcheckDir=""
keepArtifacts=0
useBaseline=0
baselineRef="HEAD"
firstCount=2000
secondCount=8000
selectedWorkloads=""

allWorkloads="c_sanity w1_readlink_err w2_subprocess_run w3_router_resolve w4_tuple w5_scheduler minimal_raise"

function usage() {
	sed -n '/^# Memory-leak spot-check/,/^# Notes:/p' "$scriptPath" | sed 's/^# \{0,1\}//'
}

function fail() {
	echo "  <<- e > memcheck: $1"
	exit 1
}

while [ $# -gt 0 ]
do
	case "$1" in
		--baseline) useBaseline=1 ;;
		--baseline-ref) baselineRef="$2"; useBaseline=1; shift ;;
		--n1) firstCount="$2"; shift ;;
		--n2) secondCount="$2"; shift ;;
		--workload) selectedWorkloads="$selectedWorkloads $2"; shift ;;
		--list) echo "$allWorkloads" | tr ' ' '\n'; exit 0 ;;
		--keep) keepArtifacts=1 ;;
		--help|-h) usage; exit 0 ;;
		*) fail "unknown option: $1 (see --help)" ;;
	esac
	shift
done

if [ -z "$selectedWorkloads" ]; then
	selectedWorkloads="$allWorkloads"
fi

[ -x "$compilerBinary" ] || fail "compiler not found at $compilerBinary (run: make -C build -j\$(nproc))"
command -v gcc >/dev/null || fail "gcc is required to build the interposer"
if [ $useBaseline -eq 1 ]; then
	command -v git >/dev/null || fail "--baseline requires git"
	git -C "$basepath" rev-parse --verify --quiet "$baselineRef" >/dev/null || fail "unknown baseline revision: $baselineRef"
fi

memcheckDir=$(mktemp -d "${TMPDIR:-/tmp}/uranite-memcheck.XXXXXX") || fail "cannot create temporary directory"

function cleanup() {
	if [ $keepArtifacts -eq 1 ]; then
		echo "  <<- i > memcheck: artifacts kept in $memcheckDir"
	else
		rm -rf "$memcheckDir"
	fi
}
trap cleanup EXIT

mkdir -p "$memcheckDir/workloads"
if [ $useBaseline -eq 1 ]; then
	mkdir -p "$memcheckDir/before-root"
	git -C "$basepath" archive "$baselineRef" stdlibs | tar -x -C "$memcheckDir/before-root" || fail "git archive of $baselineRef stdlibs failed"
fi

cat > "$memcheckDir/memtrace.c" <<'INTERPOSER'
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void* (*real_malloc)( size_t ) = NULL;
static void* (*real_calloc)( size_t, size_t ) = NULL;
static void* (*real_realloc)( void*, size_t ) = NULL;
static void (*real_free)( void* ) = NULL;

static long long alloc_count = 0;
static long long free_count = 0;
static int initializing = 0;

static char bootstrap_buffer[1 << 16];
static size_t bootstrap_used = 0;

static int is_bootstrap( void* pointer ) {
	char* casted = (char*)pointer;
	return casted >= bootstrap_buffer && casted < bootstrap_buffer + sizeof( bootstrap_buffer );
}

static void* bootstrap_alloc( size_t size ) {
	size = ( size + 15 ) & ~(size_t)15;
	if( bootstrap_used + size > sizeof( bootstrap_buffer ) ) {
		return NULL;
	}
	void* pointer = bootstrap_buffer + bootstrap_used;
	bootstrap_used += size;
	return pointer;
}

static void init_wrappers( void ) {
	if( real_calloc != NULL || initializing == 1 ) {
		return;
	}
	initializing = 1;
	real_malloc = (void* (*)(size_t))dlsym( RTLD_NEXT, "malloc" );
	real_calloc = (void* (*)(size_t, size_t))dlsym( RTLD_NEXT, "calloc" );
	real_realloc = (void* (*)(void*, size_t))dlsym( RTLD_NEXT, "realloc" );
	real_free = (void (*)(void*))dlsym( RTLD_NEXT, "free" );
	initializing = 0;
}

void* malloc( size_t size ) {
	if( real_malloc == NULL ) {
		if( initializing == 1 ) {
			return bootstrap_alloc( size );
		}
		init_wrappers();
		if( real_malloc == NULL ) {
			return bootstrap_alloc( size );
		}
	}
	void* pointer = real_malloc( size );
	__sync_fetch_and_add( &alloc_count, 1 );
	return pointer;
}

void* calloc( size_t count, size_t size ) {
	size_t total = count * size;
	if( real_calloc == NULL ) {
		void* pointer = NULL;
		if( initializing == 1 ) {
			pointer = bootstrap_alloc( total );
			if( pointer != NULL ) {
				memset( pointer, 0, total );
			}
			__sync_fetch_and_add( &alloc_count, 1 );
			return pointer;
		}
		init_wrappers();
		if( real_calloc != NULL ) {
			pointer = real_calloc( count, size );
			__sync_fetch_and_add( &alloc_count, 1 );
			return pointer;
		}
		pointer = bootstrap_alloc( total );
		if( pointer != NULL ) {
			memset( pointer, 0, total );
		}
		__sync_fetch_and_add( &alloc_count, 1 );
		return pointer;
	}
	void* pointer = real_calloc( count, size );
	__sync_fetch_and_add( &alloc_count, 1 );
	return pointer;
}

void* realloc( void* oldPointer, size_t size ) {
	if( real_realloc == NULL ) {
		init_wrappers();
	}
	if( oldPointer == NULL ) {
		return malloc( size );
	}
	if( is_bootstrap( oldPointer ) ) {
		void* pointer = malloc( size );
		if( pointer != NULL ) {
			size_t copyLimit = sizeof( bootstrap_buffer ) - (size_t)((char*)oldPointer - bootstrap_buffer);
			if( copyLimit > size ) {
				copyLimit = size;
			}
			memcpy( pointer, oldPointer, copyLimit );
		}
		return pointer;
	}
	if( real_realloc == NULL ) {
		return NULL;
	}
	void* pointer = real_realloc( oldPointer, size );
	if( pointer == NULL ) {
		return NULL;
	}
	__sync_fetch_and_add( &free_count, 1 );
	__sync_fetch_and_add( &alloc_count, 1 );
	return pointer;
}

void free( void* pointer ) {
	if( pointer == NULL ) {
		return;
	}
	if( is_bootstrap( pointer ) ) {
		return;
	}
	if( real_free == NULL ) {
		init_wrappers();
		if( real_free == NULL ) {
			return;
		}
	}
	real_free( pointer );
	__sync_fetch_and_add( &free_count, 1 );
}

__attribute__((destructor)) static void memtrace_report( void ) {
	fprintf(
		stderr,
		"[memtrace] pid=%d allocs=%lld frees=%lld outstanding=%lld\n",
		(int)getpid(),
		alloc_count,
		free_count,
		alloc_count - free_count
	);
}
INTERPOSER

gcc -shared -fPIC -O2 -o "$memcheckDir/memtrace.so" "$memcheckDir/memtrace.c" || fail "failed to build interposer"

# C reference workload: deliberately leaks one allocation per iteration.
# Must be compiled with -O0; optimization passes delete dead allocations.
cat > "$memcheckDir/c_sanity.c" <<'CSANITY'
#include <stdlib.h>
int main( void ) {
	for( long long loopIndex = 0; loopIndex < 8000; loopIndex++ ) {
		void* sink = calloc( 256, 8 );
		((long*)sink)[3] = loopIndex;
	}
	return 1;
}
CSANITY
gcc -O0 -o "$memcheckDir/c_sanity" "$memcheckDir/c_sanity.c" || fail "failed to build c_sanity"

cat > "$memcheckDir/workloads/w1_readlink_err.urn" <<'URNTPL'
package main

from uranite.io.path import readlink

public function main() -> I32:

    I64 errorCount = 0
    I64 loopIndex = 0
    while loopIndex < @N@:
        try:
            String ignoredTarget = readlink( "/proc/self/exe-does-not-exist" )
            errorCount = errorCount - 1000000
        except as error:
            errorCount = errorCount + 1
        loopIndex = loopIndex + 1
    return 0
URNTPL

cat > "$memcheckDir/workloads/w2_subprocess_run.urn" <<'URNTPL'
package main

from uranite.subprocess.subprocess import SubprocessResult, run

public function main() -> I32:

    I64 loopIndex = 0
    while loopIndex < @N@:
        SubprocessResult result = run( "/bin/true", "" )
        loopIndex = loopIndex + 1
    return 0
URNTPL

cat > "$memcheckDir/workloads/w3_router_resolve.urn" <<'URNTPL'
package main

from uranite.web.request import HttpRequest
from uranite.web.router import Router

public function main() -> I32:

    Router router = new Router()
    I64 loopIndex = 0
    while loopIndex < @N@:
        HttpRequest request = new HttpRequest( "GET", "/health", "", "" )
        router.resolve( "GET", "/health", request )
        request.destroy()
        loopIndex = loopIndex + 1
    return 0
URNTPL

cat > "$memcheckDir/workloads/w4_tuple.urn" <<'URNTPL'
package main

from uranite.collection.tuple import Tuple
from uranite.io.syscall import memoryToPtr
from uranite.memory.memory import Memory
from uranite.os.arch.native.syscall import SYS_GETCWD
from uranite.os.syscall.invoke import syscall2

public function main() -> I32:

    Memory<I64> probeBuffer = new Memory<I64>( 2 )
    I64 probeAddress = memoryToPtr( probeBuffer )
    I64 fixedCount = @N@
    I64 loopTarget = 0
    asm volatile:
        x86-64 "movq $1, $0" : output( "=r" loopTarget ) : input( "r" fixedCount ) : clobber()
        aarch64 "mov $0, $1" : output( "=r" loopTarget ) : input( "r" fixedCount ) : clobber()
    I64 checksum = 0
    I64 loopIndex = 0
    while loopIndex < loopTarget:
        syscall2( SYS_GETCWD, probeAddress, 16 )
        Tuple<I64> tupleValue = new Tuple<I64>( 8 )
        tupleValue.set( 0, loopIndex )
        checksum = checksum + tupleValue.get( 0 )
        tupleValue.destroy()
        loopIndex = loopIndex + 1
    probeBuffer.free()
    return checksum % 7
URNTPL

cat > "$memcheckDir/workloads/w5_scheduler.urn" <<'URNTPL'
package main

from uranite.async.scheduler import NativeScheduler

public function main() -> I32:

    I64 loopIndex = 0
    while loopIndex < @N@:
        NativeScheduler scheduler = new NativeScheduler()
        scheduler.destroy()
        loopIndex = loopIndex + 1
    return 0
URNTPL

cat > "$memcheckDir/workloads/minimal_raise.urn" <<'URNTPL'
package main

from uranite.errors.exception import Exception

public function thrower() -> Void:

    raise new Exception( "boom", 1, None )

public function main() -> I32:

    I64 loopIndex = 0
    while loopIndex < @N@:
        try:
            thrower()
        except as error:
            I64 suppressed = 0
        loopIndex = loopIndex + 1
    return 0
URNTPL

function measure() {
	local binary="$1"
	local pidFile="$binary.pid"
	LD_PRELOAD="$memcheckDir/memtrace.so" timeout 600 bash -c "echo \$\$ > '$pidFile'; exec '$binary'" >/dev/null 2>"$binary.err"
	local status=$?
	local pid
	pid=$(cat "$pidFile" 2>/dev/null)
	local line
	line=$(grep "pid=$pid " "$binary.err" 2>/dev/null | tail -1)
	if [ -z "$line" ]; then
		echo "- -"
		return 1
	fi
	local allocsPart="${line#*allocs=}"
	allocsPart="${allocsPart%% *}"
	local outstandingPart="${line##*outstanding=}"
	echo "$allocsPart $outstandingPart"
	return 0
}

function compileWorkload() {
	local modulesPath="$1" sourceFile="$2" outputFile="$3"
	rm -f "$outputFile"
	timeout 120 "$compilerBinary" --modules-path "$modulesPath" "$sourceFile" -o "$outputFile" >"$outputFile.compilelog" 2>&1
	local status=$?
	if [ $status -ne 0 ] || [ ! -x "$outputFile" ]; then
		tail -2 "$outputFile.compilelog" >&2
		return 1
	fi
	return 0
}

echo "  <<- i > memcheck: compiler $compilerBinary"
echo "  <<- i > memcheck: iterations $firstCount and $secondCount"
printf "%-20s %-8s %16s %16s %12s %12s\n" "workload" "stdlib" "out(n1)" "out(n2)" "slopeOUT" "slopeALLOC"
printf "%-20s %-8s %16s %16s %12s %12s\n" "--------" "------" "---------------" "---------------" "-----------" "-----------"

failures=0
for workload in $selectedWorkloads
do
	case "$workload" in
		w2_subprocess_run) countA=$((firstCount / 5)); countB=$((secondCount / 5)) ;;
				 *) countA=$firstCount; countB=$secondCount ;;
	esac
	variants="after"
	if [ $useBaseline -eq 1 ]; then
		variants="before after"
	fi
	for variant in $variants
	do
		if [ "$workload" == "c_sanity" ] || [ "$variant" == "after" ]; then
			modulesPath="$basepath/stdlibs"
		else
			modulesPath="$memcheckDir/before-root/stdlibs"
		fi
		allocsA=-1; outA=-1; allocsB=-1; outB=-1
		ok=1
		for count in $countA $countB
		do
			if [ "$workload" == "c_sanity" ]; then
				binary="$memcheckDir/c_sanity"
			else
				sourceFile="$memcheckDir/workloads/${workload}.urn"
				instantiated="$memcheckDir/${workload}_${variant}_${count}.urn"
				sed "s/@N@/$count/" "$sourceFile" > "$instantiated"
				binary="$memcheckDir/${workload}_${variant}_${count}"
				if ! compileWorkload "$modulesPath" "$instantiated" "$binary"; then
					printf "%-20s %-8s %16s %16s %12s %12s\n" "$workload" "$variant" "COMPILE" "FAIL" "-" "-"
					failures=$((failures + 1))
					ok=0
					break
				fi
			fi
			entry=$(measure "$binary")
			measureStatus=$?
			if [ $measureStatus -ne 0 ]; then
				printf "%-20s %-8s %16s %16s %12s %12s\n" "$workload" "$variant" "MEASURE" "FAIL" "-" "-"
				failures=$((failures + 1))
				ok=0
				break
			fi
			read allocsPart outstandingPart <<< "$entry"
			if [ "$count" == "$countA" ]; then
				allocsA=$allocsPart; outA=$outstandingPart
			else
				allocsB=$allocsPart; outB=$outstandingPart
			fi
		done
		if [ $ok -eq 1 ]; then
			span=$((countB - countA))
			slopeOut=$(( (outB - outA) / span ))
			slopeAlloc=$(( (allocsB - allocsA) / span ))
			printf "%-20s %-8s %16s %16s %12s %12s\n" "$workload" "$variant" "$outA" "$outB" "$slopeOut" "$slopeAlloc"
		fi
	done
done

exit $failures
