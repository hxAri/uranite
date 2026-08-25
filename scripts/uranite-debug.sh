#!/usr/bin/env bash

# 
# @author hxAri (hxari)
# @create 2026-07-28 12:47
# @update 2026-07-28 12:50
# @github https://github.com/uranite-lang/uranite/scripts/uranite-debug.sh
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

$basepath/scripts/uranite.sh --verbose --gdb-return-child-result --gdb-batch --gdb-ex "set confirm off" --gdb-ex "run" --gdb-ex "bt full" --gdb-ex "info register" --gdb-ex "quit" -O fast --run "$@"
