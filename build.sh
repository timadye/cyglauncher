#!/bin/sh
march=$(uname -m)
carch=$march
if [ "$1" = "-m32" ]; then
  shift
  carch=i686
fi
if [ "$1" = "-m64" ]; then
  shift
  carch=x86_64
fi
m=${carch}-w64-mingw32-
[ "$carch" = "$march" ] && c="" || c=${carch}-pc-cygwin-
test $# -eq 0 && set -- -Wall
set -x
${c}gcc "$@" -o escstr.o          -c escstr.c
${m}gcc "$@" -o escstr-win.o      -c escstr.c                   -mwindows
${m}gcc "$@" -o cyglaunch.exe        cyglaunch.c   escstr-win.o -mwindows
${c}gcc "$@" -o cyglaunch-cygwin.exe cyglaunch.c   escstr.o
${c}gcc "$@" -o cyglauncher.exe      cyglauncher.c escstr.o
rm escstr.o escstr-win.o
${m}strip -p cyglaunch.exe
${c}strip -p cyglaunch-cygwin.exe cyglauncher.exe
