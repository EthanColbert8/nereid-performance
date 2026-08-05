#!/usr/bin/env bash
set -e

jobs=""

while getopts ":j:" opt; do
	case "$opt" in
		j)
			if ! [[ "$OPTARG" =~ ^[0-9]+$ ]] || (( OPTARG < 1 || OPTARG > 128 )); then
				echo "error: -j must be an integer between 1 and 128" >&2
				exit 1
			fi
			jobs="$OPTARG"
			;;
		\?)
			echo "error: unsupported option -$OPTARG" >&2
			exit 1
			;;
		:)
			echo "error: option -$OPTARG requires an argument" >&2
			exit 1
			;;
	esac
done

shift $((OPTIND - 1))

if [[ $# -ne 0 ]]; then
	echo "error: build.sh accepts only the -j option" >&2
	exit 1
fi

cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if [[ -n "$jobs" ]]; then
	cmake --build build -j "$jobs"
else
	cmake --build build
fi
