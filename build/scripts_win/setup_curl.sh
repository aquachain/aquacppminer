#!/bin/bash
set -e

BUILD_PATH=libcurl_build
LIB_PATH=libcurl

echo
echo "Cleanup "
rm -rf "$LIB_PATH"
rm -rf "$BUILD_PATH"

echo
echo "Clone & compile"

# clone libcurl builder
git clone https://bitbucket.org/cryptogone/libcurl-win.git "$BUILD_PATH"

# build
cd "$BUILD_PATH"
eval "./build.bat"
cd ..

# copy build results to LIB_PATH & bin/, delete BUILD_PATH
echo
echo "Result in $LIB_PATH"
cp -r "$BUILD_PATH/third-party/libcurl" "$LIB_PATH"
rm -rf "$BUILD_PATH"


