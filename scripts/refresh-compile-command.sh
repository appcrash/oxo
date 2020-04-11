#!/bin/sh

### This script is used to refresh database of RDM, used by emacs rtags

SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT")
cd "$BASEDIR/../" && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 . && rc -J
