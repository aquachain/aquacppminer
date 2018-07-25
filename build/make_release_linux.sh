#!/bin/bash
set -e

# get version number
VERSION_NUM=v$(grep 'VERSION = "[^"]*' src/main.cpp | cut -d'"' -f 2)
VERSION="$VERSION_NUM$1"
OUT_DIR="rel/$VERSION"

echo
echo "-- Building AquaCppMiner $VERSION --"

# Cleanup
mkdir -p rel
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# Clean and Build all release versions
make -C prj/ config=rel_x64 clean aquacppminer
make -C prj/ config=relavx_x64 clean aquacppminer
make -C prj/ config=relavx2_x64 clean aquacppminer

# Copy to output folder
if ! [ -f "bin/aquacppminer" ]; then
	echo "Cannot find exe, compilation probably failed..."
	exit 1
fi

if ! [ -f "bin/aquacppminer_avx" ]; then
	echo "Cannot find AVX exe, compilation probably failed..."
	exit 1
fi

if ! [ -f "bin/aquacppminer_avx2" ]; then
	echo "Cannot find AVX2 exe, compilation probably failed..."
	exit 1
fi

cp "bin/aquacppminer" "$OUT_DIR"
cp "bin/aquacppminer_avx" "$OUT_DIR"
cp "bin/aquacppminer_avx2" "$OUT_DIR"
cp README.md "$OUT_DIR/README_${VERSION_NUM}.txt"

# tar
pushd "$OUT_DIR"
if [ -z "$1" ]; then
	tar -zcvf "../aquacppminer_${VERSION}_linux64.tar.gz" *
else
	tar -zcvf "../aquacppminer_${VERSION}.tar.gz" *
fi
popd

echo
echo "Build ok, result in $OUT_DIR"

exit 0
