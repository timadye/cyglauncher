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

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#ifdef __CYGWIN__
#include <sys/cygwin.h>
#endif
#include <windows.h>

#include "escstr.h"

DWORD ddeInstance= 0;
typedef int (*topicHandlerType)(const void *data, DWORD ldata);
char ddeServiceName[]= "cyglaunch";


const char*
myasctime()
{
  static char buf[20];
  time_t t;
  t= time(NULL);
  strftime(buf, sizeof(buf), "%Y/%m/%d-%H:%M:%S", localtime(&t));
  return buf;
}


static int
execHandler(const void *data, DWORD ldata)
{
  pid_t pid;
  char *args[1024], *argbuf, *argbuf2= NULL;
  int ntok;
  size_t i, largbuf, largbuf2= 0;

  largbuf= strlen (data)+1;
  argbuf= (char*) malloc (largbuf * sizeof(char));
  ntok= splitargs(data, args, sizeof(args)/sizeof(args[0]), argbuf, largbuf);
  if (ntok < 0) {
    fprintf(stderr, "%s: %s\n", myasctime(), (const char*) data);
    fprintf(stderr, "  -> command execution failed: too many command arguments\n");
    fflush(stderr);
    free(argbuf);
    return 0;
  }
  if (ntok == 0) {
    fprintf(stderr, "%s: null command ignored\n", myasctime());
    fflush(stderr);
    free(argbuf);
    return 0;
  }

#ifdef __CYGWIN__
  for (i= 0; i<ntok; i++) {
    if (args[i][0] == '[') {
      size_t larg;
      larg= strlen(args[i]+1);   /* length-1 */
      if (args[i][larg] == ']') {
        char* p;
        args[i][larg]= '\0';
        argbuf2= realloc(argbuf2, largbuf2+MAX_PATH);
        p= argbuf2+largbuf2;
        cygwin_conv_to_posix_path(args[i]+1, p);
#ifdef CYGLAUNCH_DEBUG
        fprintf(stderr, "%s: \"%s\" -> \"%s\"\n", myasctime(), args[i]+1, p);
#endif
        args[i]= p;
        largbuf2 += strlen (p) + 1;
      }
    }
  }
#endif

  if ((pid= fork()) == 0) {
    execvp(args[0], args);
    perror(args[0]);
    exit(127);
  } else if (pid == -1) {
    fprintf(stderr, "%s: %s\n", myasctime(), escargs(ntok, args));
    perror("  -> fork failed");
    fflush(stderr);
    free(argbuf);
    free(argbuf2);
    return 0;
  }

  fprintf(stderr, "%s-%d: %s\n", myasctime(), pid, escargs(ntok, args));
  fflush(stderr);
  free(argbuf);
  free(argbuf2);
  return 1;
}

static int
exitHandler(const void *data, DWORD ldata)
{
  PostQuitMessage(0);
  return 1;
}


char*            topics[]=        {"exec",       "exit"      };
topicHandlerType topicHandlers[]= {&execHandler, &exitHandler};
const size_t ntopics= sizeof(topics)/sizeof(topics[0]);


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
              returnPtr[i].hszSvc=   DdeCreateStringHandle(ddeInstance, ddeServiceName, CP_WINANSI);
              returnPtr[i].hszTopic= DdeCreateStringHandle(ddeInstance, topics[i],      CP_WINANSI);
            }
            returnPtr[i].hszSvc=   NULL;
            returnPtr[i].hszTopic= NULL;
            DdeUnaccessData(ddeReturn);
            return ddeReturn;
        }
    }
    return NULL;
}

void
perrorWin(const char* prefix, DWORD errnum)
{
  LPTSTR lpMsgBuf= NULL;
  if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     NULL, errnum,
                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                     (LPTSTR) &lpMsgBuf, 0, NULL))
    fprintf(stderr, "%s: %s: error number %ld", myasctime(), prefix, errnum);
  else
    fprintf(stderr, "%s: %s: %s", myasctime(), prefix, lpMsgBuf);
  if (lpMsgBuf)
    LocalFree(lpMsgBuf);
}

int
main (int argc, char* argv[])
{
  UINT err;
  HSZ ddeService= 0;
  MSG msg;
  BOOL bRet;

  err= DdeInitialize(&ddeInstance, DdeServerProc,
                     CBF_SKIP_ALLNOTIFICATIONS | CBF_FAIL_POKES | CBF_FAIL_REQUESTS, 0);
  if (err != DMLERR_NO_ERROR) {
    perrorWin("DdeInitialize error", GetLastError());
    return 1;
  }

  ddeService= DdeCreateStringHandle(ddeInstance, ddeServiceName, 0);
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
  DdeNameService(ddeInstance, NULL, 0, DNS_UNREGISTER);
  DdeUninitialize(ddeInstance);
  return msg.wParam;
}
