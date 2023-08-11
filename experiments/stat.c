#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

// gcc stat.c -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g && ./a.out
int main(void)
{
	char *path = "stat.c";
	struct stat buf;

	if (stat(path, &buf))
	{
		perror("stat");
		return EXIT_FAILURE;
	}

	printf("This file is %lld bytes\n", buf.st_size);

    return EXIT_SUCCESS;
}
