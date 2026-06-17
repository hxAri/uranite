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
	local dependencies=(
		llvm-14-dev
		libfmt-dev
		libspdlog-dev
		libgtest-dev
		cmake
		g++
	)
	# sudo apt update -y
	# sudo apt install -y ${dependencies[@]}
	if [[ $? -eq 0 ]]; then
		cmake -B build -DCMAKE_BUILD_TYPE=Release
		make -C build -j$(nproc)
	fi
}

main "$@"
exit $?
