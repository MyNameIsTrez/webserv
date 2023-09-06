#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

void sig_handler(int sig)
{
	(void)sig;
	printf("Inside sig_handler\n");
}

// Do Ctrl+C to print "Inside sig_handler"
// Do Ctrl+\ to quit the program
// NOTE: USE THE MAC TERMINAL IF THESE KEYS DON'T WORK!
// c++ signal.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	int sig = SIGINT;
	// Can't be caught or ignored
	// int sig = SIGKILL;

	// These calls both ignore the SIGINT
	signal(sig, sig_handler);
	// signal(sig, SIG_IGN);

	// Resets to the default behavior
	// signal(sig, SIG_DFL);

	// signal(sig, SIG_IGN);

	while (1)
	{
		printf("Loop\n");
		sleep(1);
	}

	return EXIT_SUCCESS;
}
