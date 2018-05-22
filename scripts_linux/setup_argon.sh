#!/bin/bash
set -e

LIB_PATH=phc-winner-argon2

echo
echo "- Cleanup -"
echo "Delete $LIB_PATH"
rm -rf "$LIB_PATH"

echo
echo "- Clone & select opt_aqua branch -"
git clone https://cryptogone@bitbucket.org/cryptogone/phc-winner-argon2-for-ario-cpp-miner.git "$LIB_PATH"
cd "$LIB_PATH"
git checkout opt_aqua
cd ..

echo
echo "- Clone Blake2B -"
rm -rf "blake2"
git clone https://cryptogone@bitbucket.org/cryptogone/blake2.git

