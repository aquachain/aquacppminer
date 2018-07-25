#!/bin/sh
set -e

VERSION_NUM=v$(grep 'VERSION = "[^"]*' src/main.cpp | cut -d'"' -f 2)
OUT_FILE="rel/aquacppminer_${VERSION_NUM}_src.zip"

git archive --format zip --output "$OUT_FILE" master

