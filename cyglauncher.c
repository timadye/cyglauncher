/*
 * cyglauncher - Cygwin launcher daemon
 *
 * Copyright (c) 2004 by Tim Adye <T.J.Adye@rl.ac.uk>.
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.
 * If this software is included in another package, acknowledgement
 * in the supporting documentation is requested, but not required.
 * This software is provided "as is" without express or implied warranty.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#ifdef __CYGWIN__
#include <signal.h>
#include <sys/cygwin.h>
#endif
#include <windows.h>

#include "escstr.h"

typedef int (*topicHandlerType)(const void *data, DWORD ldata);
static const char ddeServiceName[]= "cyglaunch";
static const char cmd_envvar[]= "CYGLAUNCH_EXEC";  /* must be upper case because Cygwin converts DOS envvars to u/c */
static const char *prog;
static DWORD ddeInstance= 0;
static int opth= 0, optH= 0;


static const char*
myasctime()
{
  static char buf[20];
  time_t t;
  t= time(NULL);
  strftime(buf, sizeof(buf), "%Y/%m/%d-%H:%M:%S", localtime(&t));
  return buf;
}

static int
spawn(size_t argc, char* const argv[])
{
  pid_t pid;
  fflush(NULL);
  if ((pid= fork()) == 0) {
#ifdef __CYGWIN__
    if (!optH) {
      sigset_t sigset;
      if (!sigemptyset(&sigset) && !sigaddset(&sigset, SIGHUP))
        sigprocmask(SIG_BLOCK, &sigset, NULL);
    }
#endif
    execvp(argv[0], argv);
    perror(argv[0]);
    exit((errno == ENOENT) ? 127 : 126);
  } else if (pid == -1) {
    fprintf(stderr, "%s: %s\n", myasctime(), escargs(argc, argv));
    perror("  -> fork failed");
    fflush(stderr);
    return 0;
  }
  fprintf(stderr, "%s-%d: %s\n", myasctime(), pid, escargs(argc, argv));
  fflush(stderr);
  return 1;
}

static int
execHandler(const void *data, DWORD ldata)
{
  char *argv[1024], *argbuf;
  int argc, ok= 0;
  size_t largbuf;

  largbuf= strlen (data)+1;
  argbuf= (char*) malloc (largbuf * sizeof(char));
  argc= splitargs(data, argv, sizeof(argv)/sizeof(argv[0]), argbuf, largbuf);
  if        (argc <  0) {
    fprintf(stderr, "%s: %s\n", myasctime(), (const char*) data);
    fprintf(stderr, "  -> command execution failed: too many command arguments\n");
    fflush(stderr);
  } else if (argc == 0) {
    fprintf(stderr, "%s: null command ignored\n", myasctime());
    fflush(stderr);
  } else {
#ifdef __CYGWIN__
    char *argbuf2= NULL;
    size_t i, largbuf2= 0;

    for (i= 0; i<argc; i++) {
      if (argv[i][0] == '[') {
        size_t larg;
        larg= strlen(argv[i]+1);   /* length-1 */
        if (argv[i][larg] == ']') {
          char* p;
          argv[i][larg]= '\0';
          argbuf2= realloc(argbuf2, largbuf2+MAX_PATH);
          p= argbuf2+largbuf2;
          cygwin_conv_to_posix_path(argv[i]+1, p);
#ifdef CYGLAUNCH_DEBUG
          fprintf(stderr, "%s: \"%s\" -> \"%s\"\n", myasctime(), argv[i]+1, p);
#endif
          argv[i]= p;
          largbuf2 += strlen (p) + 1;
        }
      }
    }
#endif

    ok= spawn((size_t) argc, argv);

#ifdef __CYGWIN__
    free(argbuf2);
#endif
  }

  free(argbuf);
  return ok;
}

static int
exitHandler(const void *data, DWORD ldata)
{
  PostQuitMessage(0);
  return 1;
}


static const char*            topics[]=        {"exec",       "exit"      };
static const topicHandlerType topicHandlers[]= {&execHandler, &exitHandler};
static const size_t ntopics= sizeof(topics)/sizeof(topics[0]);


static HDDEDATA CALLBACK
DdeServerProc (
    UINT uType,                 /* The type of DDE transaction we
                                 * are performing. */
    UINT uFmt,                  /* The format that data is sent or
                                 * received. */
    HCONV hConv,                /* The conversation associated with the
                                 * current transaction. */
    HSZ ddeTopic,               /* A string handle. Transaction-type
                                 * dependent. */
    HSZ ddeItem,                /* A string handle. Transaction-type
                                 * dependent. */
    HDDEDATA hData,             /* DDE data. Transaction-type dependent. */
    DWORD dwData1,              /* Transaction-dependent data. */
    DWORD dwData2)              /* Transaction-dependent data. */
{

    switch(uType) {
        case XTYP_CONNECT: {

            /*
             * Dde is trying to initialize a conversation with us. Check
             * and make sure we have a valid topic.
             */

            char buf[256];
            DWORD lbuf;
            size_t i;

            lbuf = DdeQueryString(ddeInstance, ddeTopic, buf, sizeof(buf), CP_WINANSI);

            for (i= 0; i<ntopics; i++) {
              if (!strcmp (buf, topics[i]))
                return (HDDEDATA) TRUE;
            }
            return (HDDEDATA) FALSE;
        }

        case XTYP_EXECUTE: {

            /*
             * Execute this script. The results will be saved into
             * a list object which will be retreived later. See
             * ExecuteRemoteObject.
             */
            char topic[256];
            DWORD ldata;
            void *data;
            size_t i;
            HDDEDATA ret= (HDDEDATA) DDE_FNOTPROCESSED;

            DdeQueryString(ddeInstance, ddeTopic, topic, sizeof(topic), CP_WINANSI);
            data= DdeAccessData(hData, &ldata);

            for (i= 0; i<ntopics; i++) {
              if (!strcmp (topic, topics[i])) {
                if ((topicHandlers[i])(data, ldata))
                  ret= (HDDEDATA) DDE_FACK;
                break;
              }
            }
            DdeUnaccessData(hData);
            return ret;
        }

        case XTYP_WILDCONNECT: {

            /*
             * Dde wants a list of services and topics that we support.
             */

            HSZPAIR *returnPtr;
            HDDEDATA ddeReturn;
            DWORD ls;
            size_t i;

            ddeReturn= DdeCreateDataHandle(ddeInstance, NULL, (ntopics+1)*sizeof(HSZPAIR), 0, 0, 0, 0);
            returnPtr = (HSZPAIR*) DdeAccessData(ddeReturn, &ls);
            for (i= 0; i<ntopics; i++) {
              returnPtr[i].hszSvc=   DdeCreateStringHandle(ddeInstance, (LPTSTR) ddeServiceName, CP_WINANSI);
              returnPtr[i].hszTopic= DdeCreateStringHandle(ddeInstance, (LPTSTR) topics[i],      CP_WINANSI);
            }
            returnPtr[i].hszSvc=   NULL;
            returnPtr[i].hszTopic= NULL;
            DdeUnaccessData(ddeReturn);
            return ddeReturn;
        }
    }
    return NULL;
}

static void
perrorWin(const char* prefix, DWORD errnum)
{
  LPTSTR lpMsgBuf= NULL;
  if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     NULL, errnum,
                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
                     (LPTSTR) &lpMsgBuf, 0, NULL))
    fprintf(stderr, "%s: %s: error number %ld", myasctime(), prefix, errnum);
  else
    fprintf(stderr, "%s: %s: %s", myasctime(), prefix, lpMsgBuf);
  if (lpMsgBuf)
    LocalFree(lpMsgBuf);
}


static const char*
parseopt(const char* p)
{
  switch (*p++) {
  case 'H':
    optH= 1;
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
  fprintf(stderr, "Usage: %s [-H] [COMMAND]\n", prog);
  return 1;
}


int
main (int argc, char* argv[])
{
  UINT err;
  HSZ ddeService= 0;
  MSG msg;
  BOOL bRet;
  size_t i;
  const char* envcmd;

  prog= argv[0];

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
        fprintf(stderr, "%s: invalid option: %s\n", prog, argv[i]);
        return 2;
      }
    }
  }

  if (opth) return usage();

  if ((envcmd= getenv(cmd_envvar))) {
    size_t lcmd= strlen(envcmd);
    char* cmd= (char*) malloc(lcmd+1);
    strcpy(cmd, envcmd);
    unsetenv(cmd_envvar);
    execHandler(cmd, lcmd);
  }

  if (argc > i)
    spawn (argc-i, argv+i);

  err= DdeInitialize(&ddeInstance, DdeServerProc,
                     CBF_SKIP_ALLNOTIFICATIONS | CBF_FAIL_POKES | CBF_FAIL_REQUESTS, 0);
  if (err != DMLERR_NO_ERROR) {
    perrorWin("DdeInitialize error", GetLastError());
    return 1;
  }

  ddeService= DdeCreateStringHandle(ddeInstance, (LPTSTR) ddeServiceName, 0);
  DdeNameService(ddeInstance, ddeService, 0L, DNS_REGISTER);

  while ((bRet= GetMessage(&msg, NULL, 0, 0)) != 0) {
    if (bRet == -1) {
      perrorWin("GetMessage error", GetLastError());
      msg.wParam= 2;
      break;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  fprintf (stderr, "%s: Exit\n", myasctime());
  DdeNameService(ddeInstance, 0L, 0L, DNS_UNREGISTER);
  DdeUninitialize(ddeInstance);
  return msg.wParam;
}
