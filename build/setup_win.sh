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
bash build/scripts_win/setup_curl.sh

cd $pwd
LOG_SECTION "RAPIDJSON"
bash build/scripts_win/setup_rapidjson.sh

cd $pwd
LOG_SECTION "ARGON"
bash build/scripts_win/setup_argon.sh

cd $pwd
LOG_SECTION "MPIR"
bash build/scripts_win/setup_mpir.sh

cd $pwd
LOG_SECTION "OPENSSL"
bash build/scripts_win/setup_openssl.sh

cd $pwd
LOG_SECTION "GENERATE VS2015 SOLUTION"
bash build/scripts_win/gen_prj.sh

LOG_SECTION "SETUP SUCCESS !"