#!/bin/bash
set -e

LIB_PATH=openssl-1.1.0f-vs2015
SEVEN_ZIP_X86="${PROGRAMFILES} (x86)/7-Zip/7z.exe"
SEVEN_ZIP_X64="${PROGRAMFILES}/7-Zip/7z.exe"

do_install=true
if $do_install; then
	echo
	echo "- Cleanup OPENSSL -"
	rm -rf "$LIB_PATH"

	echo
	echo "- Download OPENSSL -"
	curl -L -k https://www.npcglib.org/~stathis/downloads/openssl-1.1.0f-vs2015.7z > openssl.zip

	echo
	echo "- Unzip OPENSSL -"
	if [ -f "$SEVEN_ZIP_X86" ]; then
		"$SEVEN_ZIP_X86" x openssl.zip
	elif [ -f "$SEVEN_ZIP_X64" ]; then
		"$SEVEN_ZIP_X64" x openssl.zip
	else
		echo "Cannot find 7zip, please install it"
		rm openssl.zip
		exit 1
	fi
	rm openssl.zip
fi
