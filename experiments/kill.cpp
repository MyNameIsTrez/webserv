#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// c++ kill.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	pid_t pid = getpid();
	int sig = SIGKILL;
	kill(pid, sig);

	printf("This is never reached!\n");

	return EXIT_SUCCESS;
}
