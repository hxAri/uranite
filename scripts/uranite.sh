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
	
	# Puts is a pretty command line output (just for my os only)
	function puts() {
		echo -e "\x1b[0m$@"
	}
fi

function main() {
	
	function compile() {
		puts ""
		if [[ ! -d ${basepath}/build ]]; then
			puts ""
			subshell "cmake -S . -B build -DCMAKE_CXX_COMPILER=g++" "<<- b >"
			if [[ $SUBSHELLSTATUS -ne 0 ]]; then
				exit $SUBSHELLSTATUS
			fi
		fi
		# subshell "cmake --build ${basepath}/build -j$(($(nproc)/2))" "<<- c >"
		subshell "cmake --build ${basepath}/build -j$(nproc)" "<<- c >"
		return $SUBSHELLSTATUS
	}
	
	function commits() {
		puts ""
		if [[ ! -d ${basepath}/.git ]]; then
			puts "  <<- e > commits: .git: no such directory"
			return 1
		fi
		local branch=$(git rev-parse --abbrev-ref HEAD)
		if [[ -z $branch ]]; then
			puts "  <<- i > commits: branch: name not defined in config"
			stdin "branch" "<<- r > commits: branch:"
		fi
		while [[ ! $(git ls-remote origin "$branch") ]]; do
			puts "  <<- e > commits: branch: ${branch}: no such origin branch"
			branch=
			stdin "branch" "<<- r > commits: branch:"
		done
		if [[ "$branch" != "$(git branch --show-current)" ]]; then
			subshell "git checkout $branch" "<<- i > commits: git: checkout: {stdout}"
		fi
		local filenames=()
		local options=()
		if [[ $(git config --get commit.gpgSign) ]]; then
			options+=( "-S" )
		fi
		options+=( "-m" )
		local statuses=()
		puts "  <<- e > commits: scanning files"
		readarray -t statuses < <(git status --short)
		for stdout in "${statuses[@]}"; do
			local append=
			local filename="${stdout:3}"
			local messages=
			local status="${stdout:0:2}"
			status=${status/ /}
			stdin "append" "<<- r > commits: ${filename}: ${status}: add this file [Y/n]" "N"
			if [[ "${append^^}" == "Y" ]]; then
				if [[ "${status/ /}" == "M" ]]; then
					git diff "$filename"
				fi
				puts "  <<- i > commits: git: add \"$filename\""
				git add "$filename"
				stdin "messages" "<<- r > commits: ${filename}: messages:"
				puts "  <<- i > commits: git: commit ${options[@]} \"${messages//\"/\\\"}\""
				git commit ${options[@]} "$messages"
				if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
					puts "  <<- i > commits: git: something wrong when create commit"
					return 1
				fi
				filenames+=( "$filename" )
			fi
		done
		if [[ ${#filenames[@]} -ne 0 ]]; then
			puts "  <<- i > commits: git: push: origin \"$branch\""
			git push origin "$branch"
			if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
				puts "  <<- e > commits: git: push: error when push into remote repository"
				return 1
			fi
		fi
	}
	
	function listing() {
		puts ""
		puts "  <<- i > listing libraries:"
		if [[ -d ${basepath}/stdlibs ]] && [[ $(ls -A ${basepath}/stdlibs) ]]; then
			for filename in $(find ${basepath}/stdlibs); do
				if [[ -f "$filename" ]] && [[ "${filename##*.}" == "urn" ]]; then
					puts "  <<- m > ${filename/$basepath\//}"
				fi
			done
		fi
		puts ""
	}
	
	function execute() {
		compile
		local builded=$?
		local command="$basepath/build/uranite $@"
		if [[ $builded -eq 0 ]]; then
			echo -e ""
			$basepath/build/uranite "$@"
			return $?
		fi
		return $builded
	}
	
	# Handle readline with colorized prompt.
	# execute: [variable]=; stdin [variable] [prompt] [default]
	function stdin() {
		local default="$3"
		local input=
		local prompt="$(puts "$2")"
		local variable="$1"
		if [[ -z $1 ]]; then
			puts "  <<- e > stdin: error: variable name required"
			return 1
		elif [[ -z $2 ]]; then
			puts "  <<- e > stdin: error: input prompt required"
			return 1
		fi
		while [[ "$input" == "" ]]; do
			echo -n "  $prompt "
			read "input"
			if [[ -z "$input" ]] && [[ -n "$default" ]]; then
				input="$default"
			fi
		done
		if [[ ${configs[environment]} == "production" ]]; then
			eval "$variable=\"${input//\"/\\\"}\"" >> /dev/null 2>&1
		else
			eval "$variable=\"${input//\"/\\\"}\""
		fi
	}
	
	function testing() {
		puts ""
		compile
		local builded=$?
		if [[ $builded -eq 0 ]]; then
			subshell "${basepath}/build/uranite-tests -V -R $@"
			return $SUBSHELLSTATUS
		fi
		return $builded
	}
	
	local action=""
	if [[ -n "$1" ]]; then
		action=$(echo "$1" | grep -oP "\[{2}\K([^\]]+)" || echo "")
		if [[ "[[$action]]" != "$1" ]]; then
			unset action
		fi
	fi
	
	# Program argument values.
	local arguments=()
	local i=1
	for argument in "${@}"; do
		if [[ $i -ne 1 ]] || [[ -z "$action" ]]; then
			arguments+=( "$argument" )
		fi
		i=$((i+1))
	done
	
	if [[ -n "$action" ]]; then
		case "$action" in
			compile) compile ;;
			commits) commits ;;
			execute) execute "${arguments[@]}" ;;
			testing) testing "${arguments[@]}" ;;
			*)
				puts "  "
				puts "  Usage: bash \"$(basename ${__name__})\" [OPTIONS] [COMMAND] [ARGS]..."
				puts "         bash \"$(basename ${__name__})\" [[COMMAND]] [OPTIONS] [ARGS]..."
				puts "  "
				puts "    Uranite"
				puts "  "
				puts "  Commands:"
				puts "  "
				puts "    - [[compile]]       Build anc compile source code"
				puts "    - [[commits]]       Make a commit to all files except in .gitignore"
				puts "    - [[execute]]       Execute or run Uranite program"
				puts "    - [[testing]]       Testing Uranite CPP program"
				puts "  "
				puts "  Example: bash \"$(basename ${__name__})\""
				puts "           bash \"$(basename ${__name__})\" [[backing]] tar tar.gz"
				puts "  "
				if [[ "$action" != "help" ]]; then
					return 1
				fi
			;;
		esac
	else
		execute "${arguments[@]}"
	fi
	return $?
}

# Handle subshell execution.
# I don't have any idea for check last status code from subshell 
# because PIPESTATUS always give 1 status code when command work or no error.
# execute: subshell "command" "formatter"
function subshell() {
    local command="$1"
    local formatter="$2"
    if [[ "$formatter" == "" ]]; then
        formatter="<<- p >"
    fi
    formatter="$(puts "$formatter")"
    while IFS= read -r stdout; do
        if [[ $(echo -e "$stdout" | cut -d "=" -f "1" ) != "SUBSHELLSTATUS" ]]; then
            echo "  ${formatter} ${stdout//	/    }"
            continue
        fi
        eval $stdout
    done < <($command; echo -e "SUBSHELLSTATUS=${PIPESTATUS[0]}")
	return $?
}

main "$@"
exit=$?
puts ""
puts "  <<- r > $exit"
puts ""
