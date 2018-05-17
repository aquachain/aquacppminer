#!/bin/bash
clear
set -e

pwd=$(pwd)

function LOG_SECTION {
	echo
	echo "------------------------ $1"
	echo
}

LOG_SECTION "CLEANUP"
rm -rf "bin"
rm -rf "prj"
rm -rf "tools"

cd $pwd
LOG_SECTION "RAPIDJSON"
bash scripts_linux/setup_rapidjson.sh

cd $pwd
LOG_SECTION "ARGON"
bash scripts_linux/setup_argon.sh

cd $pwd
LOG_SECTION "PREMAKE"
bash scripts_linux/setup_premake.sh

cd $pwd
LOG_SECTION "GENERATE MAKEFILE"
"./tools/premake5" "gmake"

LOG_SECTION "SETUP SUCCESS !"