#!/bin/bash
set -e

if ! [ -x "$(command -v curl)" ]; then
  echo 'Error: curl not found (install with something like: sudo apt-get install curl)'
  exit 1
fi

mkdir -p tools
curl -L https://github.com/premake/premake-core/releases/download/v5.0.0-alpha12/premake-5.0.0-alpha12-linux.tar.gz > tools/premake5.tar.gz
tar xzvf tools/premake5.tar.gz -C tools/
rm tools/premake5.tar.gz
