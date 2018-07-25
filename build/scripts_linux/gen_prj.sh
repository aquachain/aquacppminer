#!/bin/bash
set -e

cp -f build/premake5.lua premake5.lua
./tools/premake5 "gmake"



