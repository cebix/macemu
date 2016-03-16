#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#define STR_MAX 1024
#define MAX_ARGV 10

FILE * run_tool(const char *if_name, const char *tool_name) {
	char cmd_buffer[STR_MAX] = {0};
	char * const argv[3] = {NULL, NULL, NULL};
	int i;
	pid_t pid, waitpid;
	int status = 0;
	int fds[2];
	char c;

	if(socketpair(PF_LOCAL, SOCK_STREAM, 0, fds) != 0) {
		fprintf(stderr, "%s: socketpair() failed: %s\n",
			__func__, strerror(errno));
		return NULL;
	}

	((const char**)argv)[0] = tool_name;
	((const char**)argv)[1] = if_name;

	/* Run sub process */
	pid = fork();
	if (pid == 0) {
		/* Child process */
		fclose(stdout);
		fclose(stdin);
		dup2(fds[0], 0);
		close(fds[1]);
		close(fds[0]);

		if (execve(tool_name, argv, NULL) < 0) {
			perror("execve");
			exit(1);
		}
	}


	close(fds[0]);

	if(read(fds[1], &c, 1) < 1) {
	  close(fds[1]);
	  return NULL;
	}

	return fdopen(fds[1], "rw");
}
