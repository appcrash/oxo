#!/bin/sh

### This script is used to refresh database of RDM, used by emacs rtags

if ! [ -x  "$(command -v rc)" ]; then
    echo "rc not found, install rtags first"
    exit
fi

SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT")
cd "$BASEDIR/../" && rc -J



# cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 
