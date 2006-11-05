#!/bin/sh -x
test $# -eq 0 && set -- -Wall
gcc "$@" -o escstr.o          -c escstr.c
gcc "$@" -o escstr-win.o      -c escstr.c                   -mwindows -mno-cygwin
gcc "$@" -o cyglaunch.exe        cyglaunch.c   escstr-win.o -mwindows -mno-cygwin
gcc "$@" -o cyglaunch-cygwin.exe cyglaunch.c   escstr.o
gcc "$@" -o cyglauncher.exe      cyglauncher.c escstr.o
rm escstr.o escstr-win.o
strip -p cyglaunch.exe cyglaunch-cygwin.exe cyglauncher.exe
