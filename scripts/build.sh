#!/bin/bash

# Usage: build.sh [CMAKE_BUILD_TYPE [<other cmake arguments>]]

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

function numproc {
	case `uname -s` in
		Linux*)
			nproc;;
		Darwin*)
			sysctl -n hw.logicalcpu;;
		*)
			echo 1;;
	esac
}

trap gotErr ERR

pushd "$( cd "$(dirname "$0")"/..; pwd -P )" > /dev/null

BUILD_TYPE=${1:-}

if [ -e build ]; then
	if [ ! -z "$BUILD_TYPE" ]; then
		# Override build type
		pushd build > /dev/null
		shift || true
		cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" .. "$@"
		popd > /dev/null
	fi
else
	if [ -z "$BUILD_TYPE" ]; then
		BUILD_TYPE=Debug
	fi

	mkdir build
	pushd build > /dev/null
	shift || true
	cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" .. "$@"
	popd > /dev/null
fi

pushd build > /dev/null
cmake --build . -- -j`numproc` all
popd > /dev/null

