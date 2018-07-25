#!/bin/bash
set -e

cp -f build/premake5.lua premake5.lua
"premake5" "vs2015"
