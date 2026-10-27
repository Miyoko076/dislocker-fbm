/* -*- coding: utf-8 -*- */
/* -*- mode: c -*- */
/*
 * Dislocker -- enables to read/write on BitLocker encrypted partitions under
 * Linux
 * Copyright (C) 2012-2013  Romain Coltel, Hervé Schauer Consultants
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#ifndef XGETOPT_h
#define XGETOPT_h

/* The C library's declarations, before the renames below */
#include <getopt.h>
#include <unistd.h>


/* Cygwin's getopt() doesn't permute argv: GNU getopt, see src/CMakeLists.txt */
#if defined(__CYGWIN) || defined(__MSYS)

extern char* dis_gnu_optarg;
extern int   dis_gnu_optind, dis_gnu_opterr, dis_gnu_optopt;

int dis_gnu_getopt(int argc, char* const argv[], const char* optstring);
int dis_gnu_getopt_long(int argc, char* const argv[], const char* optstring,
                        const struct option* longopts, int* longindex);

# define getopt      dis_gnu_getopt
# define getopt_long dis_gnu_getopt_long
# define optarg      dis_gnu_optarg
# define optind      dis_gnu_optind
# define opterr      dis_gnu_opterr
# define optopt      dis_gnu_optopt

#endif /* __CYGWIN || __MSYS */


#endif /* XGETOPT_h */
