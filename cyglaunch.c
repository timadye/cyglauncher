/*
 * cyglauncher - Cygwin launcher client
 *
 * Copyright (c) 2004 by Tim Adye <T.J.Adye@rl.ac.uk>.
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.
 * If this software is included in another package, acknowledgement
 * in the supporting documentation is requested, but not required.
 * This software is provided "as is" without express or implied warranty.
 */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#ifdef __CYGWIN__
#include <sys/cygwin.h>
#endif
#include <windows.h>

#include "escstr.h"

DWORD ddeInstance= 0;
const char ddeServiceName[]= "cyglaunch";
const char *prog= "cyglaunch";
int opte= 0, opth= 0;



static HDDEDATA CALLBACK
DdeServerProc (UINT uType, UINT uFmt, HCONV hConv, HSZ ddeTopic, HSZ ddeItem,
               HDDEDATA hData, DWORD dwData1, DWORD dwData2)
{
  /* Being a simple client application not taking care of any transaction.
     simply return NULL */

    return NULL;
}

#ifdef __CYGWIN__
#define errmsg(...) (fprintf(stderr,__VA_ARGS__), fflush(stderr))
#else
void
errmsg(const char* fmt,...)
{
  char s[4096];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(s, sizeof(s), fmt, ap);
  va_end(ap);
  MessageBox(NULL, s, NULL, 0);
}
#endif

void
perrorWin(const char* prefix, DWORD errnum)
{
  LPTSTR lpMsgBuf= NULL;
  if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     NULL, errnum,
                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                     (LPTSTR) &lpMsgBuf, 0, NULL))
    errmsg("%s: %s: error number %ld\n", prog, prefix, errnum);
  else
    errmsg("%s: %s: %s\n", prog, prefix, lpMsgBuf);
  if (lpMsgBuf)
    LocalFree(lpMsgBuf);
}

void
perrorDde(const char* prefix, DWORD errnum)
{
  const char* msg;

  switch (errnum) {
  case DMLERR_NO_CONV_ESTABLISHED:
    msg= "failed to establish connection to cyglauncher";

  case DMLERR_DATAACKTIMEOUT:
  case DMLERR_EXECACKTIMEOUT:
  case DMLERR_POKEACKTIMEOUT:
    msg= "cyglauncher did not respond";
    break;

  case DMLERR_BUSY:
    msg= "cyglauncher is busy";
    break;

  case DMLERR_NOTPROCESSED:
    msg= "cyglauncher cannot handle this command";
    break;

  default:
    msg= "DDE command failed";
  }
  errmsg("%s: %s: %s (error %ld)\n", prog, prefix, msg, errnum);
}

int
sendCommand(const char* topic, const char* command)
{
  HSZ ddeService, ddeTopic;
  HCONV ddeConv;
  HDDEDATA ddeData;
  HDDEDATA ddeReturn;

  ddeService= DdeCreateStringHandle(ddeInstance, (LPTSTR) ddeServiceName, 0);
  ddeTopic=   DdeCreateStringHandle(ddeInstance, (LPTSTR) topic, 0);
  ddeConv= DdeConnect(ddeInstance, ddeService, ddeTopic, NULL);
  if (!ddeConv) perrorDde("DdeConnect", DdeGetLastError(ddeInstance));
  DdeFreeStringHandle(ddeInstance, ddeService);
  DdeFreeStringHandle(ddeInstance, ddeTopic);
  if (!ddeConv) return 0;

  ddeData= DdeCreateDataHandle(ddeInstance, (LPTSTR) command,
                               strlen(command)+1, 0, 0, CF_TEXT, 0);
  if (!ddeData) {
    perrorDde("DdeCreateDataHandle", DdeGetLastError(ddeInstance));
    DdeDisconnect(ddeConv);
    return 0;
  }
  ddeReturn= DdeClientTransaction((LPBYTE) ddeData, 0xFFFFFFFF,
                                  ddeConv, 0, CF_TEXT, XTYP_EXECUTE, 30000, NULL);
  if (!ddeReturn) perrorDde("DdeClientTransaction", DdeGetLastError(ddeInstance));
  DdeFreeDataHandle(ddeReturn);
  DdeFreeDataHandle(ddeData);
  DdeDisconnect(ddeConv);
  if (!ddeReturn) return 0;
  return 1;
}

int
launch(const char* u)
{
  UINT err;
  err= DdeInitialize(&ddeInstance, DdeServerProc,
                     CBF_SKIP_ALLNOTIFICATIONS | CBF_FAIL_POKES | CBF_FAIL_REQUESTS, 0);
  if (err != DMLERR_NO_ERROR) {
    perrorWin("DdeInitialize error", GetLastError());
    return 1;
  }

  if (opte) {
    sendCommand("exit", u);
  } else {
#ifdef __CYGWIN__
    fprintf(stderr, "command: %s\n", u);
#endif
    sendCommand("exec", u);
  }

  DdeUninitialize(ddeInstance);
  return 0;
}

const char*
parseopt(const char* p)
{
  switch (*p++) {
  case 'e':
    opte= 1;
    break;
  case 'h':
    opth= 1;
    break;
  default:
    return NULL;
  }
  return p;
}


int
usage()
{
  errmsg("Usage: %s [-e | COMMAND]\n", prog);
  return 1;
}


#ifdef __CYGWIN__
int
main(int argc, char* argv[])
{
  const char *u;
  size_t i;

  prog= argv[0];
  if (argc < 2) return usage();

  for (i= 1; i < argc; i++) {
    const char* p;
    if (argv[i][0] != '-') break;
    if (argv[i][1] == '-' && argv[i][2] == '\0') {
      i++;
      break;
    }
    for (p= argv[i]+1; *p;) {
      p= parseopt(p);
      if (!p) {
        errmsg("%s: invalid option: %s\n", prog, argv[i]);
        return 2;
      }
    }
  }

  u= escargs(argc-i, argv+i);
  if (!u) {
    errmsg("%s: command or word too long\n", prog);
    return 2;
  }

  if (opth || (!opte && *u == '\0')) return usage();

  return launch(u);
}
#else
int APIENTRY
WinMain(HINSTANCE hInst, HINSTANCE gPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
  const char *p;
  for (p= lpCmdLine; *p; p++) {
    if (isspace(*p)) continue;
    if (*p != '-') break;
    p++;
    if (*p == '-' && (p[1] == '\0' || isspace(p[1]))) {
      p++;
      while (isspace(*p)) p++;
      break;
    }
    while (*p && !isspace(*p)) {
      const char* q= p;
      p= parseopt(p);
      if (!p) {
        errmsg("%s: invalid option: -%c\n", prog, *q);
        return 2;
      }
    }
  }

  if (opth || (!opte && *p == '\0')) return usage();

  return launch(p);
}
#endif
