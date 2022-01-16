#!/bin/bash
if [[ ! -f README.md ]]; then
	cd ..
fi

find ./chicken-coop-esp -iname *.h -o -iname *.cpp -o -iname *.c \
	-o -type d -name build -prune  | xargs clang-format --style=file -i
