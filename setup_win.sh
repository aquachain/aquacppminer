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
rm -rf "prj/debug" "prj/release"
rm -rf "prj/obj" "prj/ipch"

cd $pwd
LOG_SECTION "CURL"
bash scripts_win/setup_curl.sh

cd $pwd
LOG_SECTION "RAPIDJSON"
bash scripts_win/setup_rapidjson.sh

cd $pwd
LOG_SECTION "ARGON"
bash scripts_win/setup_argon.sh

cd $pwd
LOG_SECTION "MPIR"
bash scripts_win/setup_mpir.sh

cd $pwd
LOG_SECTION "OPENSSL"
bash scripts_win/setup_openssl.sh

cd $pwd
LOG_SECTION "GENERATE VS2015 SOLUTION"
"premake5" "vs2015"

LOG_SECTION "SETUP SUCCESS !"