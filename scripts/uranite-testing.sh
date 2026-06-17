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
	local compiled=0
	local faileds=()
	local foutputs=()
	local warnings=()
	local woutputs=()
	local executed=0
	local timeout=10
	local temporary=$(mktemp)
	for pathname in "$@"; do
		if [[ "$pathname" =~ \/$ ]]; then
			pathname="${pathname::-1}"
		fi
		if [[ -d "${basepath}/${pathname}" ]]; then
			echo -e "${basepath/${basepath}\//}/${pathname}: testing"
			for filename in "${basepath}/${pathname}"/*.urn; do
				if [[ -d "$filename" ]]; then
					echo -e "$filename/${basepath}\//: skipped because is not in the scope"
					continue
				fi
				cd "$basepath" || continue
				local binary="${filename%.urn}"
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
					echo -e "${filename/${basepath}\//}: removing intermediate code file"
				elif [[ "$filename" =~ \.(ae|c|cpp|h|hpp|sh)$ ]]; then
					continue
				else
					echo -e "${filename/${basepath}\//}: removing binary executable file"
				fi
				rm "$filename"
			fi
		done
	done
	clear
	echo -e "$temporary: removing temporary file"
	rm "$temporary"
	clear
	echo -e "=========================================="
	echo -e "$compiled: successfully compiled .urn codes"
	if [[ ${#faileds[@]} -ge 1 ]]; then
		echo -e "=========================================="
		echo -e "${#faileds[@]}: files has been error occurred"
		echo -e "=========================================="
		for i in "${!faileds[@]}"; do
			echo -e "${faileds[$i]/${basepath}\//}"
			if [[ -z "${foutputs[$i]}" ]]; then
				echo -e "(No output generated)"
			else
				echo -e "${foutputs[$i]/${basepath}\//}"
			fi
			echo -e "=========================================="
		done
	fi
	if [[ ${#warnings[@]} -ge 1 ]]; then
		if [[ ${#faileds[@]} -le 0 ]]; then
			echo -e "=========================================="
		fi
		echo -e "${#warnings[@]}: files has been warning occurred"
		echo -e "=========================================="
		for i in "${!warnings[@]}"; do
			echo -e "${warnings[$i]/${basepath}\//}"
			if [[ -z "${woutputs[$i]}" ]]; then
				echo -e "(No output generated)"
			else
				echo -e "${woutputs[$i]/${basepath}\//}"
			fi
			echo -e "=========================================="
		done
	fi
}

main "$@"
exit $?

