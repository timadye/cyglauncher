/*
 * escstr.h - command line processing routines
 *
 * Copyright (c) 2004 by Tim Adye <T.J.Adye@rl.ac.uk>.
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.
 * If this software is included in another package, acknowledgement
 * in the supporting documentation is requested, but not required.
 * This software is provided "as is" without express or implied warranty.
 */

#ifndef ESCSTR_H
#define ESCSTR_H

extern const char* escstrl(const char* s, unsigned int ls);
extern const char* escstr(const char* s);
extern const char* escargs(size_t argc, char* argv[]);
extern int         splitargs(const char* s, char** argv, size_t maxargs, char* buf, size_t maxbuf);

#endif /* ESCSTR_H */
