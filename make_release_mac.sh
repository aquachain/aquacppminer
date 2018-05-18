#!/bin/bash
set -e

REL_SUFFIX=_macOS_$(sw_vers -productVersion | cut -d '.' -f 1,2)

bash make_release_linux.sh $REL_SUFFIX
