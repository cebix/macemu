/*
 *  main.cpp - Wrapper program for standalone cxmon
 *
 *  cxmon (C) 1997-2004 Christian Bauer, Marc Hellwig
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdeps.h"

#include "mon.h"


#ifdef __BEOS__
#include <AppKit.h>
#include <KernelKit.h>
#include <StorageKit.h>
#include <stdlib.h>

// Detect if program was launched from Shell or Tracker
static bool launched_from_tracker(void)
{
	char *cmd = getenv("_");
	if (cmd == NULL || strlen(cmd) < 7)
		return false;
	return !strcmp(cmd + strlen(cmd) - 7 , "Tracker");
}

// Open Terminal window with given title for stdio, returns false on error
static bool open_stdio(const char *title)
{
	// Create key
	char key_name[64];
	bigtime_t t = system_time();
	sprintf(key_name, "%Ld", t);

	// Make pipe names
	char out_pipe_name[64], in_pipe_name[64];
	sprintf(out_pipe_name, "/pipe/debug_out_%s", key_name);
	sprintf(in_pipe_name, "/pipe/debug_in_%s", key_name);

	// Create semaphore
	char sem_name[B_OS_NAME_LENGTH], sem_id_str[B_OS_NAME_LENGTH];
	sprintf(sem_name, "debug_glue_%s", key_name);
	sem_id glue_sem = create_sem(0, sem_name);
	sprintf(sem_id_str, "%d", glue_sem);

	// Make path for "Terminal" app
	char term_path[B_PATH_NAME_LENGTH];
	find_directory(B_BEOS_APPS_DIRECTORY, -1, false, term_path, 1024);
	strcat(term_path, "/Terminal");

	// Load "Terminal"
	const char *t_argv[6];
	t_argv[0] = term_path;
	t_argv[1] = "-t";
	t_argv[2] = (char *)title;
	t_argv[3] = "/bin/debug_glue";
	t_argv[4] = key_name;
	t_argv[5] = sem_id_str;
	thread_id th = load_image(6, t_argv, (const char **)environ);
	if (th < 0) {
		delete_sem(glue_sem);
		return false;
	}

	// Start "Terminal"
	resume_thread(th);
	status_t err = acquire_sem_etc(glue_sem, 1, B_TIMEOUT, 5000000);
	delete_sem(glue_sem);
	if (err)
		return false;

	// Open input/output pipes
	FILE *in = freopen(in_pipe_name, "rb", stdin);
	if (in == NULL)
		return false;
	FILE *out = freopen(out_pipe_name, "wb", stdout);
	if (out == NULL) {
		fclose(in);
		return false;
	}

	// Set buffer modes
	setvbuf(stdout, NULL, _IOLBF, 0);
	return true;
}
#endif

// Main program
int main(int argc, const char **argv)
{
#ifdef __BEOS__
	// Launched from Tracker? Then open terminal window
	if (launched_from_tracker()) {
		if (!open_stdio("mon"))
			return 1;
	}
#endif

	// Execute mon
	mon_init();
	mon(argc, argv);
	mon_exit();
	return 0;
}
