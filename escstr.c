/*
 * escstr.c - command line processing routines
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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "escstr.h"

static const char escchar[]= "\a\b\f\n\r\t\v";
static const char escname[]= "abfnrtv";

/* escstr returns a copy of the string with unprintable characters escaped
 * and in quotes if it contains a space, comma, or quotes.
 *
 * The return string will not be overwritten by future calls until more
 * space is needed in buf.
 */

const char*
escstrl (const char* s, unsigned int ls)
{
  static char buf[4096];
  static unsigned int pos= 0;
  const char *sbuf, *pc;
  unsigned int i, j;
  int quote;

  quote= (ls==0 ? 1 : 0);
  j= pos+1;  /* Leave room for possible leading quote */
  for (i= 0; i<ls; i++) {
    char c;
#ifdef ESCSTR_INFO
    if (j+6 >= sizeof(buf)) {
#else
    if (j+2 >= sizeof(buf)) {
#endif
      if (pos>0) {
        pos= 0;
        return escstrl (s, ls);
      }
#ifdef ESCSTR_INFO
      strcpy (buf+j, "\\...");  /* \... at end indicates buffer overflow. */
      j += 4;
      break;
#else
      return NULL;
#endif
    }
    c= s[i];
    if (c>=' ' && c<='~') {
      if (c=='\"' || c==' ' || c==',') quote= 1;
      if (c=='\"' || c=='\\') buf[j++]= '\\';
      buf[j++]= c;
    } else if (c != '\0' && (pc= strchr (escchar, c))) {
      buf[j++]= '\\';
      buf[j++]= escname[pc-escchar];
    } else {
      const char* fmt= "%o";
      if (i+1<ls) {
        char cn;
        cn= s[i+1];
        if (cn>='0' && cn<='9') fmt= "%03o"; /* also 8&9, for clarity */
      }
      buf[j++]= '\\';
      j += sprintf (buf+j, fmt, (unsigned char) c);
    }
  }
  sbuf= buf+pos;
  if (quote) {
    buf[pos]= buf[j++]= '\"';
  } else {
    sbuf++;
  }
  buf[j++]= '\0';
  pos= j;
  return sbuf;
}


const char*
escstr (const char* s)
{
  return escstrl (s, strlen (s));
}


const char*
escargs(size_t argc, char* argv[])
{
  static char u[4096];
  size_t i, lu, ls;
  const char* s;

  u[0]= '\0';
  lu= 0;
  for (i= 0; i<argc; i++) {
    s= escstr(argv[i]);
    if (!s) return NULL;
    ls= strlen(s);
    if (lu+ls+2 > sizeof(u)) return NULL;
    if (lu) u[lu++]= ' ';
    memcpy(u+lu, s, ls+1);
    lu += ls;
  }
  return u;
}


/* Separate STR into arguments, respecting quote and escaped characters.
 * Returns the number of arguments (or < 0 if limits are passed), which are
 * copied into BUF and referenced from ARGV.
 */
int
splitargs (const char* s, char** argv, size_t maxargs,
           char* buf, size_t maxbuf)
{
  char c, quote= 0;
  const char *p;
  size_t argc= 0, i= 0;
  int sp= 1;

  if (!argv) return -1;
  if (!buf)  return -2;
  maxargs--;   /* leave room for final NULL */
  maxbuf--;    /* leave room for final '\0' */
  for (p= s; (c= *p); p++) {
    if (sp) {
      if (isspace (c)) continue;
      if (argc >= maxargs) return -1;
      argv[argc++]= &buf[i];
      sp= 0;
    }
    if        (c == '\\' && quote != '\'') {
      int c2;
      char* r;
      char num[4];
      c= *(++p);
      if (c == 'x') {
        strncpy (num, p+1, 2); num[2]= '\0';
        c2= (unsigned char) strtol (num, &r, 16);
        if (c2 && r && r>num) {
          p += r-num;
          c= c2;
        }
      } else if (isdigit (c)) {
        strncpy (num, p, 3); num[3]= '\0';
        c2= (unsigned char) strtol (num, &r, 8);
        if (c2 && r && r>num) {
          p += r-num-1;
          c= c2;
        }
      } else {
        r= strchr (escname, c);
        if (r) c= escchar[r-escname];
      }
    } else if (quote) {
      if (c == quote) {
          quote= 0;
          continue;
      }
    } else if (c == '\'' || c == '\"') {
      quote= c;
      continue;
    } else if (isspace (c)) {
      c= '\0';
      sp= 1;
    }
    if (i>=maxbuf) return -2;
    buf[i++]= c;
  }
  if (!sp) buf[i]= '\0';
  argv[argc]= NULL;
  return argc;
}
