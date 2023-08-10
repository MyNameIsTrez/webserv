#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// gcc kill.c -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g && ./a.out
int main(void)
{
	pid_t pid = getpid();
	int sig = SIGKILL;
	kill(pid, sig);

	printf("This is never reached!\n");

    return EXIT_SUCCESS;
}
