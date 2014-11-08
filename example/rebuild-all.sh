#!/bin/sh

EXE="/Users/andy/proj/imgtl/out/imgtl"

ROOT="$(dirname $0)"
OWD="$PWD"
for SUB in "$ROOT"/* ; do
  if [ ! -d "$SUB" ] ; then continue ; fi
  cd "$SUB"
  rm -rf output
  if ! $EXE -ooutput script ; then
    read -p "Failed at '$(basename $SUB)'. Press any key to quit."
    exit 1
  fi
  cd "$OWD"
done

open -a /Applications/Preview.app $(find "$ROOT" -name '*-FRONT.png')
