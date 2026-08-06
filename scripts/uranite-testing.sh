#!/usr/bin/env bash

#
# @author hxAri (hxari)
# @create 2026-06-17 19:34
# @update 2026-06-17 20:03
# @github https://github.com/uranite-lang/uranite
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

# Get current application basepath.
basepath=$(dirname $(pwd))

# Change current working directory into parent directory.
cd $basepath

# For compatibilty system.
if [[ ! $(command -v puts) ]]; then
	
	# echo -e is a pretty command line output (just for my os only)
	function puts() {
		echo -e "\x1b[0m$@"
	}
fi

function main() {
	local arguments=()
	local compiled=0
	local debugable=0
	local executed=0
	local faileds=()
	local foutputs=()
	local mir=0
	local timeout=10
	local totals=0
	local temporary=$(mktemp)
	local warnings=()
	local woutputs=()
	for argument in "$@"; do
		case "$argument" in
			--gdb) debugable=1 ;;
			*) arguments+=( "$argument" ) ;;
		esac
	done
	for pathname in "${arguments[@]}"; do
		if [[ "$pathname" =~ \/$ ]]; then
			pathname="${pathname::-1}"
		fi
		if [[ -d "${basepath}/${pathname}" ]]; then
			puts "${basepath/${basepath}\//}/${pathname}: testing"
			for filename in "${basepath}/${pathname}"/*.urn; do
				if [[ -d "$filename" ]]; then
					puts "$filename/${basepath}\//: skipped because is not in the scope"
					continue
				fi
				cd "$basepath" || continue
				local binary="${filename%.urn}"
				local totals=$((totals+1))
				timeout $timeout "$basepath/build/uranite" -O fast "$filename" -o "$binary" 2>&1 | tee "$temporary"
				executed=${PIPESTATUS[0]}
				if [[ "$(cat "$temporary")" =~ [Ww]arning\: ]]; then
					warnings+=( "$filename" )
					woutputs+=( "$(cat "$temporary" | grep -i "Warning:")" )
				fi
				cd "$basepath" || continue
				if [[ $executed -ne 0 ]]; then
					local status=$executed
					if [[ $executed -eq 139 ]]; then
						status="COMPILE SEGMENTATION FAULT (139)"
					elif [[ $executed -eq 124 || $executed -eq 137 ]]; then
						status="COMPILE TIMEOUT ${timeout}s ($executed)"
					else
						status="COMPILE ERROR ($executed)"
					fi
					faileds+=( "$filename (${status})" )
					foutputs+=( "$(cat "$temporary")" )
					executed=0
				else
					if [[ -f "$binary" ]]; then
						if [[ $debugable -eq 1 ]]; then
							timeout $timeout \
								gdb \
									-return-child-result \
									-batch \
									-ex "set confirm off" \
									-ex "run" \
									-ex "bt full" \
									-ex "info register" \
									-ex "quit" \
										"$binary" 2>&1 | tee "$temporary"
						else
							timeout $timeout "$binary" 2>&1 | tee "$temporary"
						fi
						executed=${PIPESTATUS[0]}
						compiled=$((compiled+1))
						if [[ $executed -ne 0 ]]; then
							local status=$executed
							if [[ $executed -eq 139 ]]; then
								status="RUNTIME SEGMENTATION FAULT (139)"
							elif [[ $executed -eq 124 || $executed -eq 137 ]]; then
								status="RUNTIME TIMEOUT ${timeout}s ($executed)"
							else
								status="RUNTIME ERROR ($executed)"
							fi
							faileds+=( "$filename (${status})" )
							foutputs+=( "$(cat "$temporary")" )
						fi
						executed=0
					else
						faileds+=( "$filename (BINARY NOT FOUND)" )
						foutputs+=( "${binary/${basepath}\//}: compilation successful, but binary not found" )
					fi
				fi
			done
		fi
		for filename in $(find "${basepath}/${pathname}"); do
			if [[ ! -f "$filename" ]]; then
				continue
			fi
			if [[ ! "$filename" =~ \.urn$ ]] || [[ -d "$filename" ]]; then
				if [[ "$filename" =~ \.ll$ ]]; then
					puts "${filename/${basepath}\//}: removing intermediate code file"
				elif [[ "$filename" =~ \.(ae|c|cpp|h|hpp|sh)$ ]]; then
					continue
				else
					puts "${filename/${basepath}\//}: removing binary executable file"
				fi
				rm "$filename"
			fi
		done
	done
	clear
	puts "$temporary: removing temporary file"
	rm "$temporary"
	puts "=========================================="
	puts "$compiled: successfully compiled of $totals .urn codes"
	if [[ ${#warnings[@]} -ge 1 ]]; then
		if [[ ${#faileds[@]} -le 0 ]]; then
			puts "=========================================="
		fi
		puts "${#warnings[@]}: files has been warning occurred of $totals totals"
		puts "=========================================="
		for i in "${!warnings[@]}"; do
			puts "${warnings[$i]/${basepath}\//}"
			if [[ -z "${woutputs[$i]}" ]]; then
				puts "(No output generated)"
			else
				puts "${woutputs[$i]/${basepath}\//}"
			fi
			puts "=========================================="
		done
	fi
	if [[ ${#faileds[@]} -ge 1 ]]; then
		puts "=========================================="
		puts "${#faileds[@]}: files has been error occurred of $totals totals"
		puts "=========================================="
		for i in "${!faileds[@]}"; do
			puts "${faileds[$i]/${basepath}\//}"
			if [[ -z "${foutputs[$i]}" ]]; then
				puts "(No output generated)"
			else
				puts "${foutputs[$i]/${basepath}\//}"
			fi
			puts "=========================================="
		done
		return 1
	fi
	return 0
}

main "$@"
exit $?

