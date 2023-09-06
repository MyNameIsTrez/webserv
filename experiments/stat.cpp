#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

// c++ stat.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	const char *path = "stat.cpp";
	stat buf;

	if (stat(path, &buf))
	{
		perror("stat");
		return EXIT_FAILURE;
	}

	printf("This file is %lld bytes\n", buf.st_size);

	return EXIT_SUCCESS;
}
