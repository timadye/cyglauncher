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
#include <shlwapi.h>

#include "escstr.h"

static const char ddeServiceName[]= "cyglaunch";
static const char cmd_envvar[]= "CYGLAUNCH_EXEC";  /* must be upper case because Cygwin converts DOS envvars to u/c */
static const char cyglauncher_envvar[]= "CYGLAUNCHER_CMD";
static const char cyglauncher_cmd[]= "cyglauncher-start";
static const char *cyglauncher_args= NULL;
static const char *prog= "cyglaunch";  /* replaced with argv[0] if known */
static DWORD ddeInstance= 0;
static int verbose= 0, opte= 0, opth= 0;

static HDDEDATA CALLBACK
DdeServerProc (UINT uType, UINT uFmt, HCONV hConv, HSZ ddeTopic, HSZ ddeItem,
               HDDEDATA hData, DWORD_PTR dwData1, DWORD_PTR dwData2)
{
  /* Being a simple client application not taking care of any transaction.
     simply return NULL */

    return NULL;
}

#ifdef __CYGWIN__
#define errmsg(...) (fprintf(stderr,__VA_ARGS__), fflush(stderr))
#define dbgmsg(...) (verbose ? (fprintf(stdout,__VA_ARGS__), fflush(stdout)) : 0)
#else
#define errmsg(...) (msgbox(0, __VA_ARGS__))
#define dbgmsg(...) (verbose ? msgbox(1, __VA_ARGS__) : 0)
static int
msgbox(int dbg, const char* fmt,...)
{
  char s[4096];
  int n;
  va_list ap;
  va_start(ap, fmt);
  n= vsnprintf(s, sizeof(s), fmt, ap);
  va_end(ap);
  MessageBox(NULL, s, dbg ? "Debug" : NULL, 0);
  return n;
}
#endif

static void
perrorWin(const char* prefix, DWORD errnum)
{
  LPTSTR lpMsgBuf= NULL;
  if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     NULL, errnum,
                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
                     (LPTSTR) &lpMsgBuf, 0, NULL))
#ifdef _WIN64
    errmsg("%s: %s: error number %u\n",  prog, prefix, errnum);
#else
    errmsg("%s: %s: error number %lu\n", prog, prefix, errnum);
#endif
  else
    errmsg("%s: %s: %s\n", prog, prefix, lpMsgBuf);
  if (lpMsgBuf)
    LocalFree(lpMsgBuf);
}

static void
perrorDde(const char* prefix, DWORD errnum)
{
  const char* msg;

  switch (errnum) {
  case DMLERR_NO_CONV_ESTABLISHED:
    msg= "failed to establish connection to cyglauncher";
    break;

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
#ifdef _WIN64
  errmsg("%s: %s: %s (error %u)\n",  prog, prefix, msg, errnum);
#else
  errmsg("%s: %s: %s (error %lu)\n", prog, prefix, msg, errnum);
#endif
}


static int
start_cyglauncher(const char* cmd)
{
  DWORD ldir;
  INT_PTR err;
  LPTSTR dir= NULL, cwd= "";
  char* launch;
  char* cmdbuf= NULL;
  char* args;
  char progpath[MAX_PATH+1], abscmd[MAX_PATH+1];
  const char* envcmd;

  if (!SetEnvironmentVariable(cmd_envvar, cmd)) {
    perrorWin("SetEnvironmentVariable error", GetLastError());
    return 1;
  }
  ldir= GetCurrentDirectory(0, NULL);
  if (ldir) {
    dir= (char*) malloc(ldir * sizeof(LPTSTR*));
    if (GetCurrentDirectory(ldir, dir)) cwd= dir;
  }

  if ((envcmd= getenv(cyglauncher_envvar))) {
    size_t lcmd= strlen(envcmd);
    launch= cmdbuf= (char*) malloc(lcmd+1);
    strcpy(launch, envcmd);
    args= strchr (launch, ' ');
    if (args) *args++= '\0';
  } else {
    launch= (char*) cyglauncher_cmd;
    args=   (char*) cyglauncher_args;
    if (PathIsRelative     (launch)                        &&
        GetModuleFileName  (0, progpath, sizeof(progpath)) &&
        PathRemoveFileSpec (progpath)                      &&
        PathCombine        (abscmd, progpath, launch)) {
      dbgmsg ("start_cyglauncher: \"%s\" -> \"%s\"\n", launch, abscmd);
      launch= abscmd;
    }
  }

  dbgmsg ("start_cyglauncher: \"%s\" \"%s\" in \"%s\"\n", launch, args ? args : "", cwd);
  err= (INT_PTR) ShellExecute (NULL, NULL, launch, args, cwd, SW_SHOWMINIMIZED);
  free(dir);
  if (err <= 32) {
    perrorWin(launch, err);
    free(cmdbuf);
    return 1;
  }
  free(cmdbuf);
  if (!SetEnvironmentVariable(cmd_envvar, NULL))
    perrorWin("SetEnvironmentVariable error (unset)", GetLastError());  /* not fatal */
  return 0;
}


static int
sendCommand(const char* topic, const char* command)
{
  HSZ ddeService, ddeTopic;
  HCONV ddeConv;
  HDDEDATA ddeData;
  HDDEDATA ddeReturn;
  UINT err;

  ddeService= DdeCreateStringHandle(ddeInstance, (LPTSTR) ddeServiceName, 0);
  ddeTopic=   DdeCreateStringHandle(ddeInstance, (LPTSTR) topic, 0);
  ddeConv= DdeConnect(ddeInstance, ddeService, ddeTopic, NULL);
  if (!ddeConv) {
    err= DdeGetLastError(ddeInstance);
    DdeFreeStringHandle(ddeInstance, ddeService);
    DdeFreeStringHandle(ddeInstance, ddeTopic);
    if (err == DMLERR_NO_CONV_ESTABLISHED) {
      if (strcmp(topic, "exec") == 0) return start_cyglauncher(command);
      if (strcmp(topic, "exit") == 0) return 1;  /* already stopped! */
    }
    perrorDde("DdeConnect", err);
    return 0;
  }
  DdeFreeStringHandle(ddeInstance, ddeService);
  DdeFreeStringHandle(ddeInstance, ddeTopic);

  ddeData= DdeCreateDataHandle(ddeInstance, (LPBYTE) command,
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

static int
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
    dbgmsg ("command: %s\n", u);
    sendCommand("exec", u);
  }

  DdeUninitialize(ddeInstance);
  return 0;
}


static const char*
parseopt(const char* p)
{
  switch (*p++) {
  case 'e':
    opte= 1;
    break;
  case 'v':
    verbose= 1;
    break;
  case 'h':
  case '?':
    opth= 1;
    break;
  default:
    return NULL;
  }
  return p;
}


static int
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
