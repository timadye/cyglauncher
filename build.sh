#!/bin/sh -x
gcc -Wall -o escstr.o          -c escstr.c
gcc -Wall -o escstr-win.o      -c escstr.c                   -mwindows -mno-cygwin
gcc -Wall -o cyglaunch.exe        cyglaunch.c   escstr-win.o -mwindows -mno-cygwin
gcc -Wall -o cyglaunch-cygwin.exe cyglaunch.c   escstr.o
gcc -Wall -o cyglauncher.exe      cyglauncher.c escstr.o
rm escstr.o escstr-win.o
strip -p cyglaunch.exe cyglaunch-cygwin.exe cyglauncher.exe
