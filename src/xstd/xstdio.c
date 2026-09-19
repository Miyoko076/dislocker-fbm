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

#include <string.h>
#include <time.h>
#include <signal.h>

#include "dislocker/xstd/xstdio.h"
#include "dislocker/xstd/xstdlib.h"
#include <readline/readline.h>   /* after xstdio.h: needs <stdio.h> first */



/* Files descriptors to know where to put logs */
static FILE* fds[DIS_LOGS_NB] = {0,};


/* Keep track of the verbosity level */
static int verbosity = L_QUIET;


/* Native Windows (HANDLE_CTRLC_THREAD): SIGINT runs on a console thread while
 * readline waits for a key, so run readline's Ctrl-C handling there at once. */
#ifdef HANDLE_CTRLC_THREAD
typedef void (*sigfn_t)(int);

static sigfn_t rl_sigint;              /* readline's own SIGINT handler */

/* Runs last: readline restores it after its cleanup and raises SIGINT. */
static void app_sigint(int sig)
{
	(void) sig;
	signal(SIGINT, SIG_IGN);           /* a 2nd Ctrl-C must not kill us mid-exit */
	exit(130);
}

static void wake_sigint(int sig)
{
	if(rl_sigint)
		rl_sigint(sig);                /* record it, as readline does */
	rl_check_signals();                /* ...and handle it now, not at the next key */
}

static void ctrlc_init(void)
{
	signal(SIGINT, app_sigint);        /* becomes readline's "original" handler */
}

int dis_ctrlc_hook(void)
{
	sigfn_t cur = signal(SIGINT, wake_sigint);

	if(cur != wake_sigint && cur != SIG_ERR && cur != SIG_DFL && cur != SIG_IGN)
		rl_sigint = cur;
	return 0;
}
#else
static void ctrlc_init(void)
{
}

int dis_ctrlc_hook(void)
{
	return 0;                          /* readline handles Ctrl-C at once */
}
#endif


/* Levels transcription into strings */
static char* msg_tab[DIS_LOGS_NB] = {
	"CRITICAL",
	"ERROR",
	"WARNING",
	"INFO",
	"DEBUG"
};



/**
 * Initialize outputs for display messages
 *
 * @param v Application verbosity
 * @param file File where putting logs (stdout if NULL)
 */
void dis_stdio_init(DIS_LOGS v, const char* file)
{
	verbosity = v;

	ctrlc_init();

	FILE* log = NULL;
	if(file)
	{
		log = fopen(file, LOG_MODE);
		if(!log)
		{
			perror("Error opening log file (falling back to stdout)");
			log = stdout;
		}
	}
	else
		log = stdout;


	switch(v)
	{
		default:
			verbosity = L_DEBUG;
			// fall through
		case L_DEBUG:
			fds[L_DEBUG] = log;
			// fall through
		case L_INFO:
			fds[L_INFO] = log;
			// fall through
		case L_WARNING:
			fds[L_WARNING] = log;
			// fall through
		case L_ERROR:
			fds[L_ERROR] = log;
			// fall through
		case L_CRITICAL:
			fds[L_CRITICAL] = log;
			break;
		case L_QUIET:
			if (log != stdout)
				fclose(log);
			break;
	}

	dis_printf(L_DEBUG, "Verbosity level to %s (%d) into '%s'\n",
	        msg_tab[verbosity], verbosity, file == NULL ? "stdout" : file);
}


/**
 * Endify in/outputs
 */
void dis_stdio_end()
{
	if(verbosity > L_QUIET)
		fclose(fds[L_CRITICAL]);
}


/**
 * Remove the '\n', '\r' or '\r\n' before the first '\0' if present
 *
 * @param string String where the '\n', '\r' or '\r\n' is removed
 */
void chomp(char* string)
{
	size_t len = strlen(string);
	if(len == 0)
		return;

	if(string[len - 1] == '\n' || string[len - 1] == '\r')
		string[len - 1] = '\0';

	if(len == 1)
		return;

	if(string[len - 2] == '\r')
		string[len - 2] = '\0';
}


/**
 * Do as printf(3) but displaying nothing if verbosity is not high enough
 * Messages are redirected to the log file if specified into xstdio_init()
 *
 * @param level Level of the message to print
 * @param format String to display (cf printf(3))
 * @param ... Cf printf(3)
 * @return The number of characters printed
 */
int dis_printf(DIS_LOGS level, const char* format, ...)
{
	int ret = -1;

	if(verbosity < level || verbosity <= L_QUIET)
		return 0;

	if(level >= DIS_LOGS_NB)
		level = L_DEBUG;


	va_list arg;
	va_start(arg, format);

	ret = dis_vprintf(level, format, arg);

	va_end(arg);

	fflush(fds[level]);

	return ret;
}


/**
 * Do as vprintf(3) but displaying nothing if verbosity is not high enough
 * Messages are redirected to the log file if specified into xstdio_init()
 *
 * @param level Level of the message to print
 * @param format String to display (cf vprintf(3))
 * @param ap Cf vprintf(3)
 */
int dis_vprintf(DIS_LOGS level, const char* format, va_list ap)
{
	if(verbosity < level || verbosity <= L_QUIET)
		return 0;

	if(level >= DIS_LOGS_NB)
		level = L_DEBUG;

	if(!fds[level])
		return 0;


	time_t current_time = time(NULL);
	char* time2string = ctime(&current_time);

	chomp(time2string);

	fprintf(fds[level], "%s [%s] ", time2string, msg_tab[level]);
	return vfprintf(fds[level], format, ap);
}
