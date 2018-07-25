#!/bin/bash
set -e

LIB_PATH="mpir-3.0.0"

do_install=true
if $do_install; then
	echo
	echo "- Cleanup MPIR -"
	rm -rf "$LIB_PATH"

	echo
	echo "- Download MPIR -"
	curl -L http://mpir.org/mpir-3.0.0.zip > mpir.zip

	echo
	echo "- Unzip MPIR -"
	unzip -q mpir.zip
	rm mpir.zip
fi

echo
echo "- Build MPIR -"
cd "$LIB_PATH/build.vc14"
./msbuild.bat gc LIB Win32 Debug
./msbuild.bat gc LIB Win32 Release
./msbuild.bat gc LIB x64 Debug
./msbuild.bat gc LIB x64 Release
cd ../..

