#!/bin/bash
set -e

# get version number
VERSION="v$(grep -Po 'VERSION = "\K[^"]*' src/main.cpp)"
OUT_DIR="rel/${VERSION}_win64"
OUT_DIR32="rel/${VERSION}_win32"
ZIP_PATH="rel/aquacpppminer_${VERSION}_win64.zip"
ZIP_PATH32="rel/aquacpppminer_${VERSION}_win32.zip"

# get 7zip path
SEVEN_ZIP_X86="${PROGRAMFILES} (x86)/7-Zip/7z.exe"
SEVEN_ZIP_X64="${PROGRAMFILES}/7-Zip/7z.exe"
if [ -f "$SEVEN_ZIP_X86" ]; then
	SEVEN_ZIP="$SEVEN_ZIP_X86"
elif [ -f "$SEVEN_ZIP_X64" ]; then
	SEVEN_ZIP="$SEVEN_ZIP_X64"
else
	echo "Cannot find 7zip, please install it"
	exit 1
fi

#get msbuild (vs2015) path
MSBUILD_VS2015="${PROGRAMFILES} (x86)\\MSBuild\\14.0\\Bin\\MSBuild.exe"
if ! [ -f "$MSBUILD_VS2015" ]; then
	echo "Cannot find msbuild, please install visual studio 2015"
	exit 1
fi

echo
echo "-- Building Aquacppminer Win32/Win64 $VERSION --"

# Cleanup
mkdir -p rel
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
rm -rf "$OUT_DIR32"
mkdir -p "$OUT_DIR32"

rm -f "$ZIP_PATH"
rm -f "$ZIP_PATH32"

# Clean and Build AVX/AVX2
cd prj
"$MSBUILD_VS2015" 'AquaCppMiner.vcxproj' '//t:Clean;Build' '//p:Configuration=Rel' '//p:Platform=win32'
"$MSBUILD_VS2015" 'AquaCppMiner.vcxproj' '//t:Clean;Build' '//p:Configuration=RelAvx' '//p:Platform=win32'
"$MSBUILD_VS2015" 'AquaCppMiner.vcxproj' '//t:Clean;Build' '//p:Configuration=RelAvx2' '//p:Platform=win32'

"$MSBUILD_VS2015" 'AquaCppMiner.vcxproj' '//t:Clean;Build' '//p:Configuration=Rel' '//p:Platform=x64'
"$MSBUILD_VS2015" 'AquaCppMiner.vcxproj' '//t:Clean;Build' '//p:Configuration=RelAvx' '//p:Platform=x64'
"$MSBUILD_VS2015" 'AquaCppMiner.vcxproj' '//t:Clean;Build' '//p:Configuration=RelAvx2' '//p:Platform=x64'
cd ..

# --------- X64: Copy to output folder & zip -------------
if ! [ -f "bin/aquacppminer_avx.exe" ]; then
	echo "Cannot find AVX exe, compilation probably failed..."
	exit 1
fi

if ! [ -f "bin/aquacppminer_avx2.exe" ]; then
	echo "Cannot find AVX2 exe, compilation probably failed..."
	exit 1
fi

if ! [ -f "bin/aquacppminer.exe" ]; then
	echo "Cannot find exe, compilation probably failed..."
	exit 1
fi

cp "bin/aquacppminer.exe" "$OUT_DIR"
cp "bin/aquacppminer_avx.exe" "$OUT_DIR"
cp "bin/aquacppminer_avx2.exe" "$OUT_DIR"
unix2dos -n readme.md "$OUT_DIR/readme_${VERSION}.txt"

"$SEVEN_ZIP" a -tzip "$ZIP_PATH" "./$OUT_DIR/*"

# --------- Win32: Copy to output folder & zip -------------
if ! [ -f "bin32/aquacppminer_avx.exe" ]; then
	echo "Cannot find AVX 32bits exe, compilation probably failed..."
	exit 1
fi

if ! [ -f "bin32/aquacppminer_avx2.exe" ]; then
	echo "Cannot find AVX2 32bits exe, compilation probably failed..."
	exit 1
fi

if ! [ -f "bin32/aquacppminer.exe" ]; then
	echo "Cannot find 32bits exe, compilation probably failed..."
	exit 1
fi

cp "bin32/aquacppminer.exe" "$OUT_DIR32"
cp "bin32/aquacppminer_avx.exe" "$OUT_DIR32"
cp "bin32/aquacppminer_avx2.exe" "$OUT_DIR32"
unix2dos -n readme.md "$OUT_DIR32/readme_${VERSION}.txt"

"$SEVEN_ZIP" a -tzip "$ZIP_PATH32" "./$OUT_DIR32/*"

# all done !
echo
echo "Build finished, result in $OUT_DIR and $OUT_DIR32"

exit 0
